# Cross-build Gens for 32-bit Windows with MinGW-w64, so that producing a
# binary needs Docker and nothing else -- Visual Studio becomes one way to
# build this, not the only way.
#
# The target is deliberately i686. Gens' CPU cores are ~32k lines of 32-bit x86
# assembly with no C fallback, and the memory dispatch tables cast pointers to
# `unsigned`; there is no 64-bit build to be had without rewriting the
# emulator. See docs/docker-build.md.
#
# Three stages:
#
#   toolchain  cross-compiler, assembler and host tools, and nothing else
#   deps       zlib and libpng cross-built and installed into the sysroot
#   builder    the Gens sources, compiled and linked
#
# The final binary is copied out by the caller; this image is not a runtime.

# ---------------------------------------------------------------------------
# toolchain
# ---------------------------------------------------------------------------
FROM --platform=linux/amd64 ubuntu:22.04 AS toolchain

RUN set -eux; \
    export DEBIAN_FRONTEND=noninteractive; \
    apt-get -y update; \
    apt-get -y install --no-install-recommends \
        build-essential ca-certificates make wget xz-utils \
        nasm \
        binutils-mingw-w64-i686 gcc-mingw-w64-i686 g++-mingw-w64-i686 \
        mingw-w64-i686-dev; \
    update-alternatives --set i686-w64-mingw32-gcc /usr/bin/i686-w64-mingw32-gcc-posix; \
    update-alternatives --set i686-w64-mingw32-g++ /usr/bin/i686-w64-mingw32-g++-posix; \
    apt-get -y clean; \
    rm -rf /var/lib/apt/lists/*

ENV CROSS=i686-w64-mingw32
ENV SYSROOT=/opt/cross

# ---------------------------------------------------------------------------
# deps: zlib and libpng. Ubuntu ships no MinGW build of either, and Gens links
# both, so they are built from source once into a sysroot the builder reuses.
# ---------------------------------------------------------------------------
FROM toolchain AS deps

ARG ZLIB_VERSION=1.3.1
ARG LIBPNG_VERSION=1.6.43

WORKDIR /tmp/build

RUN set -eux; \
    wget -q "https://zlib.net/fossils/zlib-${ZLIB_VERSION}.tar.gz"; \
    tar xf "zlib-${ZLIB_VERSION}.tar.gz"; \
    cd "zlib-${ZLIB_VERSION}"; \
    make -f win32/Makefile.gcc \
        PREFIX=${CROSS}- \
        BINARY_PATH=${SYSROOT}/bin \
        INCLUDE_PATH=${SYSROOT}/include \
        LIBRARY_PATH=${SYSROOT}/lib \
        -j"$(nproc)" install

RUN set -eux; \
    wget -q "https://download.sourceforge.net/libpng/libpng-${LIBPNG_VERSION}.tar.xz"; \
    tar xf "libpng-${LIBPNG_VERSION}.tar.xz"; \
    cd "libpng-${LIBPNG_VERSION}"; \
    ./configure \
        --host=${CROSS} \
        --prefix=${SYSROOT} \
        --enable-static --disable-shared \
        CPPFLAGS="-I${SYSROOT}/include" \
        LDFLAGS="-L${SYSROOT}/lib"; \
    make -j"$(nproc)"; \
    make install

# ---------------------------------------------------------------------------
# builder
# ---------------------------------------------------------------------------
FROM deps AS builder

WORKDIR /src
COPY . /src

RUN set -eux; \
    make -f Makefile.mingw -j"$(nproc)"; \
    ls -l build/mingw-i386/Gens.exe

# ---------------------------------------------------------------------------
# artifact: nothing but the binary, so `docker build --output` can copy it
# straight onto the host without creating a container.
# ---------------------------------------------------------------------------
FROM scratch AS artifact
COPY --from=builder /src/build/mingw-i386/Gens.exe /Gens.exe
