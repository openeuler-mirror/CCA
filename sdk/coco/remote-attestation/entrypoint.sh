#!/bin/sh

set -eu

if [ "$#" -eq 0 ]; then
    echo "usage: rats-entrypoint {kbs|as} [arguments...]" >&2
    exit 2
fi

component="$1"
shift

case "$component" in
    kbs)
        exec /usr/local/bin/kbs "$@"
        ;;
    as | grpc-as)
        exec /usr/local/bin/grpc-as "$@"
        ;;
    *)
        echo "unsupported remote attestation component: $component" >&2
        exit 2
        ;;
esac
