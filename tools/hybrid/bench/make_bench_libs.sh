#!/bin/bash
# make_bench_libs.sh IGATE_LIB_DIR MODEL_whole.c MODEL_nopath.c MODEL_info.c
# IGATE_LIB_DIR is lib/AprsCompress of LoRa_APRS_iGate (branch feature/order3-compression). Copies it three times, swapping in each table.
set -e
here="$(cd "$(dirname "$0")" && pwd)"; lib="$1"; shift
for form in whole nopath info; do
  d="$here/libs/$form/AprsCompress"; rm -rf "$d"; mkdir -p "$d/src"
  cp "$lib/library.json" "$d/"
  for f in "$lib"/src/*; do [ "$(basename "$f")" = AprsCompressModel.cpp ] || cp "$f" "$d/src/"; done
  python3 "$here/../make_firmware_model.py" "$1" > "$d/src/AprsCompressModel.cpp"; shift
done
