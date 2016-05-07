#!/bin/bash

make clean
make
sudo rmmod petmem.ko
sudo insmod petmem.ko
cd user
make clean
make
sudo dd if=/dev/zero of=/tmp/cs1651.swap bs=4096 count=256
sudo chmod +w /tmp/cs1651.swap
sudo ./petmem 128
sudo ./test
cd ..
dmesg | grep -i 'in swap_out_page' | wc -l
dmesg | grep -i 'in swap_in_page' | wc -l
