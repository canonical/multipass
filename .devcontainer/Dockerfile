FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y \
    git \
    curl \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    python3 \
    python3-pip \
    sudo \
    clang-format \
    && apt clean

WORKDIR /workspace
