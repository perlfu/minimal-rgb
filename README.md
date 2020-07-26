# Minimal RGB LED controller utilities

Minimal code for manipulating various LED controllers:

* wraith.c - AMD Wraith Prism Cooler

## Requirements

* Assumes Linux environment, but may work on other operating systems
* C compiler
* libhiddev (and development headers)

## Build

Check Makefile then:
```
make
```

## USB Access

In order to access USB devices you may need to installed the udev rules:
```
sudo cp 80-rgb.rules /etc/udev/rules.d
sudo udevadm control --reload-rules
sudo udevadm trigger
```
