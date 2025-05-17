FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    fuse \
    libfuse-dev \
    pkg-config \
    build-essential \
    inotify-tools

WORKDIR /app
COPY . /app

RUN gcc -Wall anthk.c `pkg-config fuse --cflags --libs` -o antinkfs

CMD mkdir -p /it24_host /antink_mount /var/log && \
    touch /var/log/it24.log && \
    /app/antinkfs /antink_mount -o allow_other -f
