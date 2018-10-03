FROM alpine:3.8
MAINTAINER baregl
ADD server /tmp/server
RUN apk add --no-cache --update rust cargo && \
    cd /tmp/server && \
    cargo build --release && \
    cp target/release/plybcksrv /plybcksrv

EXPOSE 9483
VOLUME ["/config", "/data"]

CMD ["/plybcksrv", "-c", "/config/plybck.toml"]
