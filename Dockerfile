FROM alpine:3.8
MAINTAINER baregl
ADD server /tmp/server
RUN apk add --no-cache --update rust cargo  libsodium libsodium-dev clang-libs clang-dev && \
    cd /tmp/server && \
    cargo build --release && \
    cp target/release/plybcksrv /plybcksrv && \
    rm -rf /tmp/server

EXPOSE 9483
VOLUME ["/config", "/data"]

CMD ["/plybcksrv", "-c", "/config/plybck.toml"]
