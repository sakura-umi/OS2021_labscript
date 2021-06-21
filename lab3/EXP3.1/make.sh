#!/bin/sh
rmmod sec2
dmesg -C
make clean
make
make clean
