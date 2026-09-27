FROM alpine:3.22 AS build

RUN apk add --no-cache build-base
COPY server.c /server.c
RUN cc -Os -ffunction-sections -fdata-sections -static \
    -Wl,--gc-sections -s -o /server /server.c

FROM scratch

COPY --from=build /server /server

USER 65534:65534
EXPOSE 8080/tcp
EXPOSE 8081/tcp
EXPOSE 8082/udp
ENTRYPOINT ["/server"]
