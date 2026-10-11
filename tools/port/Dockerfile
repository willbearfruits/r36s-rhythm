# The image `make port` and `make arm64` build in: Debian buster for ARM64. Its glibc (2.28) is older than ArkOS's,
# so what it builds runs there. Make it once (an x86 PC needs qemu-user-static for ARM64 containers):
#   docker build --platform linux/arm64 -t r36s-build:buster tools/port
FROM debian:buster
RUN printf 'deb http://archive.debian.org/debian buster main\ndeb http://archive.debian.org/debian-security buster/updates main\n' > /etc/apt/sources.list \
 && apt-get -o Acquire::Check-Valid-Until=false update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends build-essential make pkg-config libsdl2-dev libasound2-dev file \
 && rm -rf /var/lib/apt/lists/*
