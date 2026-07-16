#!/usr/bin/env bash
# Source this once per terminal:  source ./idf-env.sh
export IDF_PYTHON_ENV_PATH="${HOME}/.espressif/python_env/idf5.5_py3.13_env"
# shellcheck disable=SC1091
source "${HOME}/esp/esp-idf/export.sh"
export ESPPORT="${ESP_PORT:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)}"
echo "ESP-IDF ready. Port: ${ESPPORT:-NOT FOUND — plug in ESP32}"
