#!/bin/bash
set -e

cp tests/disk_images/a.img a2.img

# Check folder structure
echo "Checking root directory"
./ds3ls a2.img /
echo "Making files"

# Making files
./ds3touch a2.img 0 F.txt
./ds3ls a2.img /

# Test order of copying files
# 1. Copy A to F
# 2. Copy A to F again
# 3. Copy B to F
# 4. Copy C to F
# 5. Copy B to F
# 6. Copy A to F
# Copy D to F
# Note: can also test file taking up between one and two blocks of space/do other test cases

echo "First Test: Copy small file to F.txt"
./ds3cp a2.img tests/A.txt 4
./ds3cat a2.img 4
echo -e "\n"


echo "Second Test: Copy small file to F.txt again"
./ds3cp a2.img tests/A.txt 4
./ds3cat a2.img 4
echo -e "\n"

echo "Third Test: Copy file that takes one block of space"
./ds3cp a2.img tests/B.txt 4
./ds3cat a2.img 4
echo -e "\n"

echo "Fourth Test: Copy file that takes two block of space"
./ds3cp a2.img tests/C.txt 4
./ds3cat a2.img 4
echo -e "\n"

echo "Fifth Test: Copy file that takes one block of space again"
./ds3cp a2.img tests/B.txt 4
./ds3cat a2.img 4
echo -e "\n"

echo "Sixth Test: Copy file that takes one small amout of space again"
./ds3cp a2.img tests/A.txt 4
./ds3cat a2.img 4
echo -e "\n"

echo "Seventh Test: Copy empty file"
./ds3cp a2.img tests/D.txt 4
./ds3cat a2.img 4
echo -e "\n"

echo "Test complete"