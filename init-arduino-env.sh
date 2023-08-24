#!/bin/sh

repository_base_path=$(realpath "$0" | xargs dirname)

export ARDUINO_DIRECTORIES_DATA="$repository_base_path/.arduino"
export ARDUINO_DIRECTORIES_DOWNLOADS="$repository_base_path/.arduino/staging"
export ARDUINO_DIRECTORIES_USER="$repository_base_path/src"
