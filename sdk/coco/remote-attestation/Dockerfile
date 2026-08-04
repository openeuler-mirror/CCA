# SPDX-License-Identifier: Apache-2.0

FROM ubuntu:24.04

ARG RATS_UID=10001
ARG RATS_GID=10001

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends -y \
        ca-certificates \
        libgcc-s1 \
        libssl3 \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --gid "${RATS_GID}" rats \
    && useradd --no-create-home --uid "${RATS_UID}" --gid "${RATS_GID}" \
        --shell /usr/sbin/nologin rats

COPY bin/kbs bin/grpc-as /usr/local/bin/
COPY entrypoint.sh /usr/local/bin/rats-entrypoint

RUN chmod 0755 \
        /usr/local/bin/kbs \
        /usr/local/bin/grpc-as \
        /usr/local/bin/rats-entrypoint

USER ${RATS_UID}:${RATS_GID}

ENTRYPOINT ["/usr/local/bin/rats-entrypoint"]
