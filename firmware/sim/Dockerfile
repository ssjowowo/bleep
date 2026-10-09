# Build image for the emulator: compiler, SDL2, and the icon-font tools.
# Used by sim/test.sh; not needed if the packages are installed natively.
FROM ubuntu:24.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkg-config libsdl2-dev git ca-certificates python3 python3-pip python3-venv \
 && rm -rf /var/lib/apt/lists/*
RUN python3 -m venv /opt/py && /opt/py/bin/pip install --no-cache-dir fonttools picosvg
ENV PATH=/opt/py/bin:$PATH
