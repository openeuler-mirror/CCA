# Copyright (c) 2021 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

ARG IMAGE_REGISTRY=docker.io
FROM ${IMAGE_REGISTRY}/ubuntu:24.04

ENV GO_HOME="/opt"
ENV GOCACHE="${GO_HOME}/.cache"
ENV RUSTUP_HOME="/opt/rustup"
ENV CARGO_HOME="/opt/cargo"
ENV PATH="/opt/cargo/bin/:/opt/go/bin:${PATH}"

ARG GO_VERSION="1.22.11"
ARG GO_DOWNLOAD_MIRRORS="https://mirrors.aliyun.com/golang https://golang.google.cn/dl https://go.dev/dl"
ARG GO_SHA256=""
ARG RUST_TOOLCHAIN="1.90.0"
ARG ARCH="aarch64"

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN mkdir ${RUSTUP_HOME} ${CARGO_HOME} ${GOCACHE} && \
    chmod -R a+rwX ${RUSTUP_HOME} ${CARGO_HOME} ${GO_HOME}

# makedev tries to mknod from postinst
RUN [ -x /usr/bin/systemd-detect-virt ] || ( echo "echo docker" >/usr/bin/systemd-detect-virt && chmod +x /usr/bin/systemd-detect-virt )
# hadolint ignore=DL3009,SC2046
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive \
    apt-get --no-install-recommends -y install \
    ca-certificates \
    curl \
    g++ \
    $(gcc_arch="${ARCH}" && [ "$(uname -m)" != "$gcc_arch" ] && ( \
         libc_arch="$gcc_arch" && \
         [ "$gcc_arch" = aarch64 ] && libc_arch=arm64; \
         [ "$gcc_arch" = ppc64le ] && gcc_arch=powerpc64le && libc_arch=ppc64el; \
         [ "$gcc_arch" = s390x ] && gcc_arch=s390x && libc_arch=s390x; \
         [ "$gcc_arch" = x86_64 ] && gcc_arch=x86-64 && libc_arch=amd64; \
         echo "gcc-$gcc_arch-linux-gnu libc6-dev-$libc_arch-cross")) \
    git \
    gnupg2 \
    libclang-dev \
    make \
    makedev \
    multistrap \
    musl \
    musl-dev \
    musl-tools \
    protobuf-compiler \
    xz-utils \
    pip \
    python3-dev \
    libclang-dev \
    zstd \
    ninja-build \
    vim \
    bzip2 \
    pkg-config \
    libglib2.0-dev \
    libmount-dev \
	libssl-dev

COPY certs/ /usr/local/share/ca-certificates/
RUN update-ca-certificates

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --default-toolchain ${RUST_TOOLCHAIN}

RUN ARCH=$(uname -m); \
    goarch=""; \
    go_sha256=""; \
    kernelname=$(uname -s | tr '[:upper:]' '[:lower:]'); \
    case "${ARCH}" in \
        "aarch64") goarch="arm64"; go_sha256="9ebfcab26801fa4cf0627c6439db7a4da4d3c6766142a3dd83508240e4f21031" ;; \
        "ppc64le") goarch=${ARCH}; go_sha256="963a0ec973640b23ee8bb7a462cc415276fd8436111a03df8c34eb3b1ae29f12" ;; \
        "x86_64") goarch="amd64"; go_sha256="0fc88d966d33896384fbde56e9a8d80a305dc17a9f48f1832e061724b1719991" ;; \
        "s390x") goarch=${ARCH}; go_sha256="1a235afe650dee989fb37fef6aa520f35e4cd557c31453f3e82b553da3a90669" ;; \
        *) echo "Unsupported architecture: ${ARCH}" && exit 1 ;; \
    esac; \
    if [ "${GO_VERSION}" != "1.22.11" ] && [ -z "${GO_SHA256}" ]; then \
        echo "GO_SHA256 must be set when overriding GO_VERSION" >&2; \
        exit 1; \
    fi; \
    [ -z "${GO_SHA256}" ] || go_sha256="${GO_SHA256}"; \
    go_archive="go${GO_VERSION}.${kernelname}-${goarch}.tar.gz"; \
    for mirror in ${GO_DOWNLOAD_MIRRORS}; do \
        echo "Downloading ${go_archive} from ${mirror}"; \
        if curl --fail --location --retry 3 --retry-all-errors \
            --connect-timeout 15 --output "${go_archive}" \
            "${mirror}/${go_archive}"; then \
            break; \
        fi; \
        rm -f "${go_archive}"; \
    done; \
    [ -s "${go_archive}" ] || { echo "Failed to download ${go_archive}" >&2; exit 1; }; \
    echo "${go_sha256}  ${go_archive}" | sha256sum --check -; \
    tar -C "${GO_HOME}" -xzf "${go_archive}"; \
    rm "${go_archive}"

# aarch64 requires this name -- link for all
RUN if [ ! -f "/usr/bin/$(uname -m)-linux-musl-gcc" ]; then ln -s /usr/bin/musl-gcc "/usr/bin/$(uname -m)-linux-musl-gcc"; fi

RUN ARCH=$(uname -m); \
	rust_arch=""; \
    libc=""; \
    case "${ARCH}" in \
        "aarch64") rust_arch="${ARCH}"; libc="musl"; ;; \
        "ppc64le") rust_arch="powerpc64le"; libc="gnu"; ;; \
        "x86_64") rust_arch="${ARCH}"; libc="musl"; ;; \
        "s390x") rust_arch="${ARCH}"; libc="gnu"; ;; \
        *) echo "Unsupported architecture: ${ARCH}" && exit 1 ;; \
	esac; \
    rustup target add "${rust_arch}-unknown-linux-${libc}"

RUN pip install --no-cache-dir pyinstaller==6.9.0 --break-system-packages
RUN pip install Sphinx --break-system-packages
