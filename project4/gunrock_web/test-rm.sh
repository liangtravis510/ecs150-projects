#!/bin/bash
cp tests/disk_images/a.img a2.img

echo "setting up file"
./ds3ls a2.img /a/b
./ds3rm a2.img 2 c.txt
./ds3ls a2.img /a/b
./ds3bits a2.img

cp tests/disk_images/b.img b2.img