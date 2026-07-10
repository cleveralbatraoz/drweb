FROM debian:trixie-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends cmake g++ g++-multilib make nasm

WORKDIR /src
COPY . .

RUN cmake -S . -B build && cmake --build build

CMD ["ctest", "--test-dir", "build", "--output-on-failure"]
