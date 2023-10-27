#!/bin/sh
set -x
echo "Running Credentials-fetcher flatpak"

cd /app/bin

mkdir -p /var/credentials-fetcher/
mkdir -p /var/credentials-fetcher/socket/
mkdir -p /var/credentials-fetcher/krbdir/
mkdir -p /var/credentials-fetcher/logging/

./credentials-fetcherd -s gmsa-plugin-input
