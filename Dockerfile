# Shared layer with dependencies
FROM alpine:3.21.3 AS shared

RUN apk add --no-cache \
    lua5.4 \
    make \
    openssl \
    pcre2



# Build dependencies
FROM shared AS build-deps

RUN apk add --no-cache \
    alpine-sdk \
    lua5.4-dev \
    openssl-dev \
    pcre2-dev



# Build layer
FROM build-deps AS build

RUN git clone https://github.com/lefcha/imapfilter.git /dist
WORKDIR /dist
RUN make \
    INCDIRS=-I/usr/include/lua5.4 \
    LDFLAGS='-L/usr/lib/lua5.4' \
    LIBLUA=-llua \
    LUA_CFLAGS=-I/usr/include/lua5.4



# Final layer with installed imapfilter
FROM shared AS final

COPY --from=build /dist /dist
WORKDIR /dist
RUN make install \
    && rm -rf /dist

RUN addgroup abc \
    && adduser \
        --home /home/imapfilter \
        --shell /sbin/nologin \
        --disabled-password \
        --ingroup abc \
        abc

USER abc:abc

ENTRYPOINT [ "/usr/local/bin/imapfilter" ]
CMD ["-c", "/config.lua"]
