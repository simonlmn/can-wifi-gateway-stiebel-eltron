#!/bin/sh

./build-all.sh

rm -rf dist
mkdir dist
cp src/serial-can-bridge/build/*/serial-can-bridge.ino.hex "dist/serial-can-bridge-$new_tag.hex"
cp src/wifi-gateway/build/*/wifi-gateway.ino.bin "dist/wifi-gateway-$new_tag.bin"
