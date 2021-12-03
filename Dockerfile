FROM ubuntu:latest

COPY . /imapfilter

RUN apt update && \
    DEBIAN_FRONTEND="noninteractive" apt install -y build-essential ca-certificates libpcre2-dev liblua5.3-dev libssl-dev && \
    cd /imapfilter && \
    make INCDIRS=-I/usr/include/lua5.3/ LIBDIRS=-L/usr/lib/x86_64-linux-gnu/ LIBLUA=-llua5.3 && \
    make install && \
    apt remove -y build-essential && \
    apt autoremove -y && \
    rm -R /imapfilter

CMD /usr/local/bin/imapfilter -c /config.lua
