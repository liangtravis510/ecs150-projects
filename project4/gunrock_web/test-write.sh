#!/bin/bash

# Print BEFORE message
echo "START\n"

cp tests/disk_images/a.img a2.img

# Initial state
./ds3cat ./a2.img 3
echo "\n"
./ds3bits ./a2.img 

# Write a file to the disk
echo "AFTER LONG WRITE\n"
./ds3mkdir ./a2.img 1
echo "\n"
./ds3cat ./a2.img 3
echo "\n"
./ds3bits ./a2.img 

# Write another file to the disk
echo "AFTER SHORT WRITE\n"
./ds3mkdir ./a2.img 0
echo "\n"
./ds3cat ./a2.img 3
echo "\n"
./ds3bits ./a2.img

# Delete the first file
echo "AFTER DELETING LONG WRITE\n"
./ds3rm ./a2.img 1
echo "\n"
./ds3cat ./a2.img 3
echo "\n"
./ds3bits ./a2.img

# Delete the second file
echo "AFTER DELETING SHORT WRITE\n"
./ds3rm ./a2.img 0
echo "\n"
./ds3cat ./a2.img 3
echo "\n"
./ds3bits ./a2.img