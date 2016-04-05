#!/bin/bash

# Get latest code
git pull

# Clean up old
make clean

# Build new server binary
make -j4 server

# Get restart code from config file
code=`jq -r '.["restart-code"]' config.json`

# Build restart URL
url="https://petscan.wmflabs.org/restart?$code"

# Restart server
curl -s -o /dev/null $url
echo "Server restarted!"
