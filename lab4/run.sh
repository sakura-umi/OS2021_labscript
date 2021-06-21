#!/bin/sh
fusermount -qzu /home/os/lab4/1/fat_dir
cd /home/os/lab4/1
make clean
make
./simple_fat16 -d /home/os/lab4/1/fat_dir
