#!/bin/bash

# Demonstrate a complete mess of effects at the same time.

wraith \
  "mirage 1 1 1" \
  "ring-map rainbow rainbow rainbow chase chase chase chase chase swirl swirl swirl morse morse" \
  "ring-effect rainbow 5 0xff 0xff 0xff 0xff" \
  "ring-effect chase 3 0xff 0x00 0xff 0x00" \
  "ring-effect swirl 3 0xff 0x00 0x00 0xff" \
  "ring-effect morse 4 0xff 0xff 0xff 0x00" \
  "effect logo breath 5 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0x80" \
  "effect fan breath 3 0x90 0xff 0xff 0xff"