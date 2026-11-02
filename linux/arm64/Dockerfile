# The arm64 Linux build of Arcade Foundry: Insert Coin and its cores
# (scripts/build_arm64.py builds and runs this). Debian 12 (Bookworm), the
# base of Raspberry Pi OS 12, so what it builds runs on Bookworm and newer.
#  - On an arm64 host (an Apple silicon Mac with Docker Desktop or OrbStack,
#    an arm64 Linux machine) the image is arm64 and builds natively.
#  - On an x86-64 host it is x86-64 and cross-compiles with GCC's aarch64
#    toolchain and Debian's arm64 multiarch libraries (aarch64-toolchain.cmake).
FROM debian:bookworm
ARG DEBIAN_FRONTEND=noninteractive
# SDL 3's backends (the app), zlib and OpenGL/EGL/GLES (the cores), SDL 2 and
# SDL2_net (Model 3 cores)
ARG LIBS="libasound2-dev libpulse-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev \
    libxi-dev libxss-dev libxtst-dev libxkbcommon-dev libdrm-dev libgbm-dev libegl-dev libgl-dev libgles-dev \
    libglu1-mesa-dev libwayland-dev libdecor-0-dev libudev-dev libdbus-1-dev zlib1g-dev libsdl2-dev libsdl2-net-dev"
RUN set -e; \
    if [ "$(dpkg --print-architecture)" = arm64 ]; then \
        extra="$LIBS"; \
    else \
        dpkg --add-architecture arm64; \
        extra="crossbuild-essential-arm64 qemu-user zlib1g-dev $(for p in $LIBS; do printf '%s:arm64 ' "$p"; done)"; \
    fi; \
    apt-get update; \
    apt-get install -y --no-install-recommends build-essential cmake ninja-build python3 git pkg-config \
        ca-certificates curl xz-utils $extra; \
    rm -rf /var/lib/apt/lists/*; \
    git config --system --add safe.directory '*'
