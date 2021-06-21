#!/bin/sh
rmmod sec3
dmesg -C
make clean
make
make clean
