#!/bin/bash
cp tests/disk_images/a.img a2.img

# First test
echo "First test: removing an empty directory"
./ds3mkdir a2.img 0 testdir1
echo "Directory listing before removal:"
./ds3ls a2.img /
echo "Attempting to remove 'testdir1'"
./ds3rm a2.img 0 testdir1
echo "Directory listing after removal:"
./ds3ls a2.img /
echo -e "\n"

# Second test
echo "Second test: removing a empty file"
cp tests/disk_images/a.img a2.img
./ds3touch a2.img 0 testfile1
echo "Directory listing before removal:"
./ds3ls a2.img /
echo "Attempting to remove 'testfile1'"
./ds3rm a2.img 0 testfile1
echo "Directory listing after removal:"
./ds3ls a2.img /
echo -e "\n"

# Third test
echo "Third test: removing a non-empty file"
cp tests/disk_images/a.img a2.img

# Directory size before adding files
echo "Directory size:"
./ds3bits a2.img

# Add files
./ds3touch a2.img 0 testfile2
./ds3cp a2.img tests/B.txt 4
./ds3cat a2.img 4

# Directory size after adding files
echo "Directory size:"
./ds3bits a2.img

echo "Attempting to remove 'testfile2'"
./ds3rm a2.img 0 testfile2
echo "Directory listing after removal:"
./ds3ls a2.img /

# Directory size after removing files
echo "Directory size:"
./ds3bits a2.img
echo -e "\n"

echo "Fourth test: Remove entry that is not at the end of the directory data region"
cp tests/disk_images/a.img a2.img
./ds3touch a2.img 0 testfile3
./ds3touch a2.img 0 testfile4
./ds3touch a2.img 0 testfile5
echo "Directory listing before removal:"
./ds3ls a2.img /
echo "Attempting to remove 'testfile4'"
./ds3rm a2.img 0 testfile4
echo "Directory listing after removal:"
./ds3ls a2.img /
echo -e "\n"