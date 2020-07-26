#!/bin/bash

# Demonstrate a static setup.
# Set everything to red, but turn off half of the ring LEDs.

wraith \
  "mirage 0 0 0" \
  "ring-map off static off static off static static off static off static off static off static" \
  "ring-effect static 1 0xff 0xff 0x00 0x00" \
  "effect logo static 1 0xff 0xff 0x00 0x00" \
  "effect fan static 1 0xa0 0xff 0x00 0x00"
