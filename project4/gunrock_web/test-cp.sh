#!/bin/bash

echo "Running ds3cp tests..."


# # Test 1: Copy a small file
# echo "Test 1: Copy a small file"
# cp tests/disk_images/a.img a2.img
# ./ds3cp a2.img tests/A.txt 3
# ./ds3bits a2.img
# ./ds3ls a2.img /a/b
# echo -e "\n"

# Test 2: Copy a large file
echo "Test 2: Copy a large file"
cp tests/disk_images/a.img a2.img
echo "Before copying ds3bits:"
./ds3bits a2.img
./ds3cp a2.img tests/C.txt 3
echo -e "\n"
echo "After copying ds3bits:"
./ds3bits a2.img
./ds3ls a2.img /a/b
echo -e "\n"

# # Test 3: Overwriting an existing file
# echo "Test 3: Overwriting an existing file"
# cp tests/disk_images/a.img a2.img 
# ./ds3cp a2.img tests/A.txt 3
# ./ds3cp a2.img tests/B.txt 3
# ./ds3bits a2.img
# ./ds3ls a2.img /a/b
# echo -e "\n"

# # Test 4: Copy a file to a non-empty directory
# cp tests/disk_images/a.img a2.img 
# ./ds3mkdir a2.img 0 newdir
# ./ds3cp a2.img tests/A.txt 4
# ./ds3bits a2.img
# ./ds3ls a2.img /a/b
# echo -e "\n"

# # Test 5: Insufficient space in data region
# echo "Test 5: Insufficient space in data region"
# cp tests/disk_images/a.img a2.img
# for i in {1..28}; do
#     ./ds3touch a2.img 0 testfile$i
# done
# ./ds3cp a2.img tests/A.txt 3
# ./ds3bits a2.img
# ./ds3ls a2.img /a/b
# echo -e "\n"

# # Test 6: Invalid Source file
# echo "Test 6: Invalid Source file"
# cp tests/disk_images/a.img a2.img
# ./ds3cp a2.img tests/invalid.txt 3
# ./ds3bits a2.img
# ./ds3ls a2.img /a/b
# echo -e "\n"

# # Test 7: Invalid Destination file
# echo "Test 7: Invalid Destination file"
# cp tests/disk_images/a.img a2.img
# ./ds3cp a2.img tests/A.txt 9999
# ./ds3bits a2.img
# ./ds3ls a2.img /a/b