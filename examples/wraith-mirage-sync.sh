#!/bin/bash

# Demonstrate mirage sync to fan speed on Wraith Prism.
# This ca

# XXX: Set this to the sysfs fan input
INPUT="/sys/devices/platform/nct6775*/hwmon/hwmon*/fan2_input"

# Put red and blue out of phase so they "move" at different rates.
# Set them all the same to freeze the blades.
RPM=`cat $INPUT`
let RHZ=$RPM-2
let GHZ=$RPM
let BHZ=$RPM+2

wraith \
  "mirage $RHZ $GHZ $BHZ" \
  "effect fan static 1 0xff 0xff 0xff 0xff"
