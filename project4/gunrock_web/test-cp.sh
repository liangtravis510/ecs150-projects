#!/bin/bash
set -e

cp tests/disk_images/b.img test.img

./ds3cp test.img tests/A.txt 5
./ds3ls test.img /a/b
./ds3cp test.img tests/B.txt 6
