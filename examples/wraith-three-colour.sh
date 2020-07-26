#!/bin/bash

# Demonstrate three static ring colours on Wraith Prism.
#
# The ring only supports one static colour,
# but we fake two more using breath and morse channels
# with background colours and blending modes.

wraith \
  "ring-map static static static static static breath breath breath breath breath morse morse morse morse morse" \
  "ring-effect static 1 0xff 0xff 0x00 0x00" \
  "ring-effect breath 1 0xff 0x00 0xff 0x00 0x00 0xff 0x00 0x40" \
  "ring-effect morse 1 0xff 0x00 0x00 0xff 0x00 0x00 0xff"
