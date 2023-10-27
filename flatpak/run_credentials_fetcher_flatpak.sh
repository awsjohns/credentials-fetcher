#!/bin/sh
set -x
echo "Running Credentials-fetcher flatpak"

cd /app/bin

mkdir -p /var/credentials-fetcher/
mkdir -p /var/credentials-fetcher/socket/
mkdir -p /var/credentials-fetcher/krbdir/
mkdir -p /var/credentials-fetcher/logging/

export PATH=$PATH:/app/lib/sdk/dotnet6/bin
export DOTNET_ROOT=/app/lib/sdk/dotnet6/lib
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
export SASL_PATH=/app/lib/sasl2
#LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/app/lib/sdk/dotnet6/lib

./credentials-fetcherd -s gmsa-plugin-input
