# The C++ bot on Ubuntu 24.04: the same libraries as on macOS/Homebrew, from apt.
FROM ubuntu:24.04 AS build
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        cmake ninja-build g++ pkg-config ca-certificates \
        libboost-dev libssl-dev libsimdjson-dev libzmq3-dev cppzmq-dev libsecp256k1-dev \
        libgtest-dev libbenchmark-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
# -j2: Beast/Asio translation units need ~1 GB of RAM each; small servers have 2-4 GB.
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j2 \
    && ctest --test-dir build --output-on-failure

FROM ubuntu:24.04
# The -dev packages pull in exactly the runtime libraries the binary links against.
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates libssl-dev libsimdjson-dev libzmq3-dev libsecp256k1-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /src/build/hft_bot /src/build/hft_verify /usr/local/bin/
COPY abi ./abi
COPY config ./config
# Monitor mode on Arbitrum One; RPC endpoints come from the environment (ARBITRUM_RPC_URL, ...).
ENTRYPOINT ["hft_bot"]
CMD ["--config", "config/arbitrum.json"]
