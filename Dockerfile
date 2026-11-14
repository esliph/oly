FROM gcc:14

# clang-format-22 and not Debian's default (19): .clang-format uses keys that 19 rejects
# (BinPackParameters: OnePerLine, SortIncludes as a map, BinPackLongBracedList).
# Headers for SDL3's X11 and Wayland video backends; SDL loads the libraries at run time.
RUN apt-get update \
  && apt-get install -y --no-install-recommends cmake git python3 linux-perf clang-format-22 \
    pkg-config libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev \
    libxtst-dev libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev libegl-dev libgl-dev libdrm-dev \
    libgbm-dev \
  && rm -rf /var/lib/apt/lists/* \
  && ln -s /usr/bin/clang-format-22 /usr/local/bin/clang-format

WORKDIR /app
ENV OLY_TOOLCHAIN=dev
