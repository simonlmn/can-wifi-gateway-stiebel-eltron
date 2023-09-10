#!/bin/sh

export REPOSITORY_BASE_PATH=$(realpath "$0/../..")

for directory in "$REPOSITORY_BASE_PATH"/src/*/; do
    "$REPOSITORY_BASE_PATH/tools/generate-version.sh" "$directory"
    for directory in "$directory"*/; do
        if [ -d "$directory" ]; then
            "$REPOSITORY_BASE_PATH/tools/generate-version.sh" "$directory"
        fi
    done
done
