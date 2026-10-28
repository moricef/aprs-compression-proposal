#!/usr/bin/env python3
"""Convert a table written by train_layout_model.py (reference-codec format) into the iGate firmware's AprsCompressModel.cpp.
  make_firmware_model.py MODEL.c > AprsCompressModel.cpp"""
import sys
m = open(sys.argv[1]).read()
m = m.replace('#include "aprs_ppm_model.h"', '#include "AprsCompressModel.h"\n\nnamespace AprsCompressModel {')
m = m.replace('const uint8_t aprs_ppm_model_version_value', 'const uint8_t VERSION').replace('aprs_ppm_symbols[]', 'SYMBOLS[]').replace('aprs_ppm_counts[]', 'COUNTS[]')
m = m.replace('const aprs_ppm_order_t aprs_ppm_orders[APRS_PPM_ORDER + 1]', 'const OrderTable ORDERS[ORDER + 1]')
sys.stdout.write(m + '\n}\n')
