#!/bin/bash

# Reset the disk image
cp tests/disk_images/a.img a2.img
./ds3ls a2.img /a/b
./ds3ls a2.img /
./ds3bits a2.img

# Test 1: Copy a small file
echo "Test 1: Copy a small file"
./ds3touch a2.img 0 smallfile
echo "hello" > small.txt
./ds3cp a2.img small.txt 4
./ds3cat a2.img 4
./ds3bits a2.img


