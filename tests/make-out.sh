#!/bin/bash
set -e

cd "$(dirname "$0")"

if [ ! -e out ]; then
    mkdir -p /tmp/syc-out
    ln -s /tmp/syc-out out
fi
