#!/bin/sh

export REPOSITORY_BASE_PATH=$(realpath "$0/..")
. "$REPOSITORY_BASE_PATH/tools/init-arduino-env.sh"

"$REPOSITORY_BASE_PATH/tools/generate-all-versions.sh"

arduino-cli compile --profile default "$REPOSITORY_BASE_PATH/src/serial-can-bridge"

"$REPOSITORY_BASE_PATH/tools/build-wifi-gateway-ui.sh"

"$REPOSITORY_BASE_PATH/tools/generate-version.sh" "$REPOSITORY_BASE_PATH/src/wifi-gateway"

arduino-cli compile --profile default "$REPOSITORY_BASE_PATH/src/wifi-gateway"
