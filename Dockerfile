FROM alpine AS builder

RUN apk --no-cache add make gcc lua lua-dev openssl openssl-dev libc-dev pcre pcre-dev pcre2-dev

WORKDIR /build

ADD . /build
RUN make && make install

FROM alpine

COPY --from=builder /usr/local/bin/imapfilter /usr/local/bin/imapfilter
COPY --from=builder /usr/local/share/imapfilter /usr/local/share/imapfilter

RUN apk --no-cache add lua lua-dev openssl pcre git \
    && adduser -D -u 1000 imapfilter

USER imapfilter

ENTRYPOINT ["/usr/local/bin/imapfilter"]
