#!/bin/bash
cp tests/disk_images/a.img a2.img


# Test script for ds3touch
DISK_IMAGE="a2.img"  # Update with the path to your disk image
DS3MKDIR="./ds3mkdir"  # Update with the path to your ds3mkdir binary

# Utility function to run a command and print its result
run_test() {
    echo "Running: $@"
    $@ && echo "Test passed." || echo "Test failed."
    echo "------------------------------------"
}

# 1. Test creating a new directory in the root directory
echo "Test 1: Create a new directory in the root directory"
run_test $DS3MKDIR $DISK_IMAGE 0 testdir1

# 2. Test creating a directory inside another directory
echo "Test 2: Create a directory inside another directory"
run_test $DS3MKDIR $DISK_IMAGE 1 nested_dir  # Assuming inode 1 corresponds to an existing directory

# 3. Test creating a directory that already exists
echo "Test 3: Create a directory that already exists"
run_test $DS3MKDIR $DISK_IMAGE 0 testdir1

# 4. Test creating a directory in a non-existent directory
echo "Test 4: Create a directory in a non-existent directory"
run_test $DS3MKDIR $DISK_IMAGE 999 nonexistent_dir  # Assuming inode 999 doesn't exist

# 5. Test creating a directory with an invalid name
echo "Test 5: Create a directory with an invalid name"
run_test $DS3MKDIR $DISK_IMAGE 0 "invalid:dir"

echo "All tests completed."
