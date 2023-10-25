#!/bin/sh
set -x
echo "Running Credentials-fetcher flatpak"

cd /app/bin

mkdir /var/credentials-fetcher/
mkdir /var/credentials-fetcher/socket/
mkdir /var/credentials-fetcher/krbdir/
mkdir /var/credentials-fetcher/logging/

./credentials-fetcherd
