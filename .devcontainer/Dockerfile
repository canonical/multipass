FROM ubuntu:26.04
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
 && apt-get install --yes --no-install-recommends \
       build-essential \
       ca-certificates \
       cmake \
       curl \
       devscripts \
       dnsmasq-base \
       dnsmasq-utils \
       equivs \
       git \
       iproute2 \
       iptables \
       iputils-ping \
       libatm1 \
       libxtables12 \
       libgl1 \
       libltdl-dev \
       libpng16-16 \
       libslang2 \
       ninja-build \
       nodejs \
       npm \
       pkg-config \
       qemu-system-x86 \
       qemu-utils \
       rustup \
       sudo \
       unzip \
       xterm \
 && rm -rf /var/lib/apt/lists/*

# Optional: install Debian build-deps at image build time.
COPY debian/control /tmp/debian-control
RUN apt-get update \
 && mk-build-deps \
        --install \
        --remove \
        --tool "apt-get -y --no-install-recommends" \
        /tmp/debian-control \
 && rm -f /tmp/debian-control /tmp/*build-deps*.deb \
 && rm -rf /var/lib/apt/lists/*

RUN npm install -g @github/copilot

# Ensure ubuntu user exists and can sudo.
RUN id ubuntu >/dev/null 2>&1 || useradd -m -s /bin/bash ubuntu \
 && echo "ubuntu ALL=(ALL) NOPASSWD:ALL" >/etc/sudoers.d/ubuntu \
 && chmod 0440 /etc/sudoers.d/ubuntu

USER ubuntu
WORKDIR /home/ubuntu

# Install rust stable toolchain.
RUN rustup default stable

# Install caveman (to make agents less chatty, and therefore less expensive).
RUN curl -fsSL https://raw.githubusercontent.com/JuliusBrussee/caveman/main/install.sh | bash
