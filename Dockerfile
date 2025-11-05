FROM debian:bookworm-slim as arm_build_image

RUN apt-get update && \
    apt-get install -y \
    gcc-arm-none-eabi \
    git \
    libbsd-dev \
    build-essential \
    wget \
    iputils-ping \
    iproute2 \
    curl \
    flake8 \
    python3-pip \
    python3-numpy \
    python3-yaml \
    python3-serial \
    python3-setuptools-scm \
    openocd \
    pkg-config && \
    rm -rf /var/lib/apt/lists/*

# Allow pip to install packages
RUN mkdir -p $HOME/.config/pip && \
    printf "[global]\nbreak-system-packages = true\n" > \
        $HOME/.config/pip/pip.conf && \
    cat $HOME/.config/pip/pip.conf
