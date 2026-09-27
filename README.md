# test
Tiny container ID server.

Supports HTTP/TCP/UDP.

Pull the image:
```sh
docker pull ghcr.io/radito/test
```

Run one of these commands.
Stop it with Ctrl+C; `--rm` removes the stopped container.

**All Ports:**

```sh
docker run --rm -p 8080-8081:8080-8081 -p 8082:8082/udp ghcr.io/radito/test
```

**HTTP/1.1 only**

```sh
docker run --rm -p 8080:8080 ghcr.io/radito/test
```

**Raw TCP only**

```sh
docker run --rm -p 8081:8081 ghcr.io/radito/test
```

**Raw UDP only**

```sh
docker run --rm -p 8082:8082/udp ghcr.io/radito/test
```

The response is Docker's default short container ID followed by a newline.
HTTP uses HTTP/1.1 headers; TCP and UDP send only the raw bytes.


Try the ports with:
```sh
curl -i --http1.1 http://localhost:8080/
nc localhost 8081
echo ping | nc -u -w 1 localhost 8082
```

## Supported architectures

`linux/arm64`, `linux/amd64`, `linux/amd64/v2`, `linux/riscv64`, `linux/ppc64le`, `linux/s390x`, `linux/386`, `linux/mips64le`, `linux/mips64`, `linux/arm/v7`, `linux/arm/v6`.

The runtime image uses `scratch` and contains one static C binary.
To build it locally for your machine:

```sh
docker build -t test .
```
