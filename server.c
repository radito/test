#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static volatile sig_atomic_t stopping;

static void on_signal(int signal_number) {
    (void)signal_number;
    stopping = 1;
}

static int bind_socket(int type, uint16_t port) {
    int fd = socket(AF_INET, type, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    int reuse = 1;
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse) < 0 ||
        bind(fd, (struct sockaddr *)&address, sizeof address) < 0 ||
        (type == SOCK_STREAM && listen(fd, 64) < 0)) {
        perror("bind/listen");
        close(fd);
        return -1;
    }
    return fd;
}

static int send_all(int fd, const char *data, size_t length) {
    while (length > 0) {
        ssize_t sent = send(fd, data, length, MSG_NOSIGNAL);
        if (sent < 0 && errno == EINTR) {
            continue;
        }
        if (sent <= 0) {
            return -1;
        }
        data += sent;
        length -= (size_t)sent;
    }
    return 0;
}

static void read_http_headers(int fd) {
    struct timeval timeout = {.tv_sec = 2};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

    char input[1024];
    const char *end = "\r\n\r\n";
    size_t matched = 0;
    size_t total = 0;
    while (total < 8192 && !stopping) {
        ssize_t count = recv(fd, input, sizeof input, 0);
        if (count <= 0) {
            return;
        }
        total += (size_t)count;
        for (ssize_t i = 0; i < count; i++) {
            matched = input[i] == end[matched] ? matched + 1 : input[i] == '\r';
            if (matched == 4) {
                return;
            }
        }
    }
}

static void serve_tcp(int listener, const char *body, size_t body_length,
                      const char *headers, size_t headers_length) {
    int client = accept(listener, NULL, NULL);
    if (client < 0) {
        return;
    }
    if (headers != NULL) {
        read_http_headers(client);
        send_all(client, headers, headers_length);
    }
    send_all(client, body, body_length);
    close(client);
}

static void serve_udp(int listener, const char *body, size_t body_length) {
    char input[2048];
    struct sockaddr_storage peer;
    socklen_t peer_length = sizeof peer;
    ssize_t received = recvfrom(listener, input, sizeof input, 0,
                                (struct sockaddr *)&peer, &peer_length);
    if (received < 0) {
        return;
    }
    while (sendto(listener, body, body_length, 0,
                  (struct sockaddr *)&peer, peer_length) < 0 && errno == EINTR) {
    }
}

int main(void) {
    char hostname[256];
    if (gethostname(hostname, sizeof hostname) < 0) {
        perror("gethostname");
        return 1;
    }
    hostname[sizeof hostname - 1] = '\0';

    char body[sizeof hostname + 1];
    int body_length = snprintf(body, sizeof body, "%s\n", hostname);
    char headers[256];
    int headers_length = snprintf(headers, sizeof headers,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n", body_length);
    if (body_length < 0 || body_length >= (int)sizeof body ||
        headers_length < 0 || headers_length >= (int)sizeof headers) {
        return 1;
    }

    struct sigaction action = {0};
    action.sa_handler = on_signal;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    int http = bind_socket(SOCK_STREAM, 8080);
    int tcp = bind_socket(SOCK_STREAM, 8081);
    int udp = bind_socket(SOCK_DGRAM, 8082);
    if (http < 0 || tcp < 0 || udp < 0) {
        if (http >= 0) close(http);
        if (tcp >= 0) close(tcp);
        if (udp >= 0) close(udp);
        return 1;
    }

    printf("Container %s listening on HTTP :8080, TCP :8081, UDP :8082\n", hostname);
    fflush(stdout);

    struct pollfd sockets[] = {
        {.fd = http, .events = POLLIN},
        {.fd = tcp, .events = POLLIN},
        {.fd = udp, .events = POLLIN},
    };
    while (!stopping) {
        int ready = poll(sockets, 3, -1);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }
        if (sockets[0].revents & POLLIN)
            serve_tcp(http, body, (size_t)body_length, headers, (size_t)headers_length);
        if (sockets[1].revents & POLLIN)
            serve_tcp(tcp, body, (size_t)body_length, NULL, 0);
        if (sockets[2].revents & POLLIN)
            serve_udp(udp, body, (size_t)body_length);
    }

    close(http);
    close(tcp);
    close(udp);
    return 0;
}
