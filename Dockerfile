
# Stage 1: Build cmarket with C++20.
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        libboost-dev \
        libssl-dev \
        nlohmann-json3-dev \
        ca-certificates && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cmake -S . -B build-docker \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF && \
    cmake --build build-docker \
        --target cmarket \
        --parallel 2

# Stage 2: Minimal runtime environment.
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        libssl3t64 \
        libstdc++6 && \
    rm -rf /var/lib/apt/lists/*

COPY --from=builder \
    /app/build-docker/cmarket \
    /usr/local/bin/cmarket

RUN useradd --system --create-home cmarket

USER cmarket

WORKDIR /home/cmarket

ENTRYPOINT ["/usr/local/bin/cmarket"]
