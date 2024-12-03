#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <assert.h>

#include "LocalFileSystem.h"
#include "ufs.h"

using namespace std;

LocalFileSystem::LocalFileSystem(Disk *disk)
{
  this->disk = disk;
}

void LocalFileSystem::readSuperBlock(super_t *super)
{
  char buffer[UFS_BLOCK_SIZE];
  disk->readBlock(0, buffer);
  memcpy(super, buffer, sizeof(super_t));
}

void LocalFileSystem::readInodeBitmap(super_t *super, unsigned char *inodeBitmap)
{
  for (int blockNumber = 0; blockNumber < super->inode_bitmap_len; blockNumber++)
  {
    disk->readBlock(super->inode_bitmap_addr + blockNumber, inodeBitmap + (blockNumber * UFS_BLOCK_SIZE));
  }
}

void LocalFileSystem::writeInodeBitmap(super_t *super, unsigned char *inodeBitmap)
{
  for (int blockNumber = 0; blockNumber < super->inode_bitmap_len; blockNumber++)
  {
    disk->writeBlock(super->inode_bitmap_addr + blockNumber, inodeBitmap + (blockNumber * UFS_BLOCK_SIZE));
  }
}

void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap)
{
  for (int blockNumber = 0; blockNumber < super->data_bitmap_len; blockNumber++)
  {
    disk->readBlock(super->data_bitmap_addr + blockNumber, dataBitmap + (blockNumber * UFS_BLOCK_SIZE));
  }
}

void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap)
{
  for (int blockNumber = 0; blockNumber < super->data_bitmap_len; blockNumber++)
  {
    disk->writeBlock(super->data_bitmap_addr + blockNumber, dataBitmap + (blockNumber * UFS_BLOCK_SIZE));
  }
}

void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes)
{
  int inodesPerBlock = UFS_BLOCK_SIZE / sizeof(inode_t);
  for (int blockNumber = 0; blockNumber < super->inode_region_len; blockNumber++)
  {
    disk->readBlock(super->inode_region_addr + blockNumber, inodes + inodesPerBlock * blockNumber);
  }
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes)
{
  int inodesPerBlock = UFS_BLOCK_SIZE / sizeof(inode_t);
  for (int blockNumber = 0; blockNumber < super->inode_region_len; blockNumber++)
  {
    disk->writeBlock(super->inode_region_addr + blockNumber, inodes + inodesPerBlock * blockNumber);
  }
}

int LocalFileSystem::lookup(int parentInodeNumber, std::string name)
{
  // Read the superblock
  super_t super;
  readSuperBlock(&super);

  // Get the parent inode
  inode_t parentInode;
  int statResult = this->stat(parentInodeNumber, &parentInode);
  if (statResult != 0)
  {
    return statResult;
  }

  //] Check if the parent inode is a directory
  if (parentInode.type != UFS_DIRECTORY)
  {
    return -EINVALIDINODE; // Parent inode is not a directory
  }

  // Validate directory size alignment
  if (parentInode.size % sizeof(dir_ent_t) != 0)
  {
    return -EINVALIDINODE; // Directory size is corrupted
  }

  // Step 5: Read the contents of the directory
  char buffer[parentInode.size];
  int readBytes = this->read(parentInodeNumber, buffer, parentInode.size);
  if (readBytes < 0)
  {
    return readBytes;
  }

  // Iterate through directory entries
  int offset = 0;
  while (offset < readBytes)
  {
    dir_ent_t *entry = reinterpret_cast<dir_ent_t *>(buffer + offset);

    // Validate the entry's inode number and name
    if (entry->inum != -1 && name == std::string(entry->name))
    {
      return entry->inum; // Found the entry, return its inode number
    }

    offset += sizeof(dir_ent_t); // Move to the next entry
  }

  // Return error if name is not found
  return -ENOTFOUND;
}

int LocalFileSystem::stat(int inodeNumber, inode_t *inode)
{
  // Read the superblock
  super_t super;
  readSuperBlock(&super); // Use the helper function to read the superblock

  // Validate the inode number
  if (inodeNumber < 0 || inodeNumber >= super.num_inodes)
  {
    return -EINVALIDINODE;
  }

  // Read the inode bitmap
  unsigned char inode_bitmap[super.inode_bitmap_len * UFS_BLOCK_SIZE];
  readInodeBitmap(&super, inode_bitmap);

  // Calculate the bit position for the inode in the bitmap
  int byte_offset = inodeNumber % 8; // Bit offset within the byte
  int bitmap_byte = inodeNumber / 8; // Byte index in the bitmap
  char bitmask = 0b1 << byte_offset;

  // Check if the inode is allocated
  if (!(inode_bitmap[bitmap_byte] & bitmask))
  {
    return -ENOTALLOCATED; // Inode exists but is not allocated
  }

  // Read the inode region
  inode_t inodes[super.inode_region_len * UFS_BLOCK_SIZE / sizeof(inode_t)];
  readInodeRegion(&super, inodes);

  // Retrieve the inode metadata and populate the output parameter
  *inode = inodes[inodeNumber];
  return 0; // Success
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size)
{
  if (size < 0)
  {
    return -EINVALIDSIZE;
  }

  // Check existence of inode
  super_t super;
  readSuperBlock(&super);
  if (inodeNumber < 0 || inodeNumber >= super.num_inodes)
  {
    return -EINVALIDINODE;
  }

  // Read the inode bitmap and check allocation
  unsigned char inode_bitmap[super.inode_bitmap_len * UFS_BLOCK_SIZE];
  readInodeBitmap(&super, inode_bitmap);
  int byte_offset = inodeNumber % 8;
  int bitmap_byte = inodeNumber / 8;
  char bitmask = 1 << byte_offset;
  if (!(inode_bitmap[bitmap_byte] & bitmask))
  {
    return -EINVALIDINODE;
  }

  // Read the inode region
  inode_t inodes[super.inode_region_len * UFS_BLOCK_SIZE / sizeof(inode_t)];
  readInodeRegion(&super, inodes);
  inode_t inode = inodes[inodeNumber];

  // Check inode size validity for directories
  if (inode.type == UFS_DIRECTORY && size % sizeof(dir_ent_t))
  {
    return -EINVALIDSIZE;
  }

  // Read the data blocks
  int bytesRead = 0;
  int bytesToRead = min(size, inode.size);
  int blockIndex = 0;
  unsigned char blockBuffer[UFS_BLOCK_SIZE];

  while (bytesRead < bytesToRead && blockIndex < DIRECT_PTRS)
  {
    if (inode.direct[blockIndex] == 0)
    {
      break; // No more data blocks
    }

    this->disk->readBlock(inode.direct[blockIndex], blockBuffer);

    int bytesInBlock = min(UFS_BLOCK_SIZE, bytesToRead - bytesRead);
    memcpy(static_cast<char *>(buffer) + bytesRead, blockBuffer, bytesInBlock);

    bytesRead += bytesInBlock;
    blockIndex++;
  }

  return bytesRead;
}

int LocalFileSystem::create(int parentInodeNumber, int type, std::string name)
{
  // Read super block
  super_t super;
  readSuperBlock(&super);

  // Validate parent inode
  inode_t parentInode;
  int statResult = this->stat(parentInodeNumber, &parentInode);
  if (statResult != 0)
  {
    return statResult;
  }

  if (parentInode.type != UFS_DIRECTORY)
  {
    return -EINVALIDINODE; // Parent is not a directory
  }

  // Check if name is valid
  if (name.length() >= DIR_ENT_NAME_SIZE)
  {
    return -EINVALIDNAME; // Name is too long
  }

  // Check if name already exists
  int existingInode = this->lookup(parentInodeNumber, name);
  if (existingInode >= 0)
  {
    inode_t existingInodeData;
    this->stat(existingInode, &existingInodeData);
    if (existingInodeData.type == type)
    {
      return existingInode; // Name already exists with correct type
    }
    else
    {
      return -EINVALIDTYPE; // Name exists with a conflicting type
    }
  }

  // Allocate new inode
  unsigned char inodeBitmap[super.inode_bitmap_len * UFS_BLOCK_SIZE];
  readInodeBitmap(&super, inodeBitmap);

  int newInodeNumber = -1;
  for (int i = 0; i < super.num_inodes; ++i)
  {
    int byte_offset = i % 8;
    int bitmap_byte = i / 8;
    char bitmask = 0b1 << byte_offset;

    if (!(inodeBitmap[bitmap_byte] & bitmask))
    {
      newInodeNumber = i;
      inodeBitmap[bitmap_byte] |= bitmask; // Mark inode as allocated
      break;
    }
  }

  if (newInodeNumber == -1)
  {
    return -ENOTENOUGHSPACE; // No free inodes available
  }

  writeInodeBitmap(&super, inodeBitmap);

  // Initilize new Inode
  inode_t newInode = {};
  newInode.type = type;
  newInode.size = (type == UFS_DIRECTORY) ? 2 * sizeof(dir_ent_t) : 0;

  if (type == UFS_DIRECTORY)
  {
    unsigned char dataBitmap[super.data_bitmap_len * UFS_BLOCK_SIZE];
    readDataBitmap(&super, dataBitmap);

    int freeBlock = -1;
    for (int j = 0; j < super.num_data; ++j)
    {
      int byte_offset = j % 8;
      int bitmap_byte = j / 8;
      char bitmask = 0b1 << byte_offset;

      if (!(dataBitmap[bitmap_byte] & bitmask))
      {
        freeBlock = j;
        dataBitmap[bitmap_byte] |= bitmask; // Mark block as allocated
        break;
      }
    }

    if (freeBlock == -1)
    {
      return -ENOTENOUGHSPACE; // No free blocks available
    }

    writeDataBitmap(&super, dataBitmap);

    dir_ent_t entries[2];
    strncpy(entries[0].name, ".", DIR_ENT_NAME_SIZE - 1);
    entries[0].name[DIR_ENT_NAME_SIZE - 1] = '\0'; // Null-terminate
    entries[0].inum = newInodeNumber;

    strncpy(entries[1].name, "..", DIR_ENT_NAME_SIZE - 1);
    entries[1].name[DIR_ENT_NAME_SIZE - 1] = '\0'; // Null-terminate
    entries[1].inum = parentInodeNumber;

    this->write(super.data_region_addr + freeBlock, entries, sizeof(entries));
    newInode.direct[0] = super.data_region_addr + freeBlock;
  }

  inode_t inodes[super.inode_region_len * UFS_BLOCK_SIZE / sizeof(inode_t)];
  readInodeRegion(&super, inodes);
  inodes[newInodeNumber] = newInode;
  writeInodeRegion(&super, inodes);

  // Add the entry to the parent directory
  dir_ent_t newEntry = {};
  strncpy(newEntry.name, name.c_str(), DIR_ENT_NAME_SIZE - 1);
  newEntry.name[DIR_ENT_NAME_SIZE - 1] = '\0'; // Null-terminate
  newEntry.inum = newInodeNumber;

  this->write(parentInodeNumber, &newEntry, sizeof(newEntry)); // Write at the end of the directory

  // Update the parent directory inode
  parentInode.size += sizeof(dir_ent_t); // Increase size for the new entry
  inodes[parentInodeNumber] = parentInode;
  writeInodeRegion(&super, inodes);

  return newInodeNumber;
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size)
{
  if (size < 0)
  {
    return -EINVALIDSIZE;
  }

  super_t super;
  char superBlockBuffer[UFS_BLOCK_SIZE];
  disk->readBlock(0, superBlockBuffer);
  memcpy(&super, superBlockBuffer, sizeof(super_t));

  if (inodeNumber < 0 || inodeNumber >= super.num_inodes)
  {
    return -EINVALIDINODE;
  }

  int inodesPerBlock = UFS_BLOCK_SIZE / sizeof(inode_t);
  int inodeBlock = super.inode_region_addr + (inodeNumber / inodesPerBlock);
  int inodeOffset = (inodeNumber % inodesPerBlock) * sizeof(inode_t);
  char inodeBlockBuffer[UFS_BLOCK_SIZE];
  disk->readBlock(inodeBlock, inodeBlockBuffer);
  inode_t inode;
  memcpy(&inode, inodeBlockBuffer + inodeOffset, sizeof(inode_t));

  if (inode.type != UFS_REGULAR_FILE)
  {
    return -EWRITETODIR;
  }

  int requiredBlocks = (size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;

  // Reset invalid block pointers
  for (int i = 0; i < DIRECT_PTRS; i++)
  {
    if (inode.direct[i] <= 0 ||
        static_cast<unsigned int>(inode.direct[i]) >= static_cast<unsigned int>(super.data_region_addr + super.num_data))
    {
      inode.direct[i] = 0;
    }
  }

  char dataBitmapBuffer[UFS_BLOCK_SIZE];
  disk->readBlock(super.data_bitmap_addr, dataBitmapBuffer);

  // Allocate blocks
  for (int i = 0; i < requiredBlocks; i++)
  {
    if (inode.direct[i] == 0)
    {
      bool blockAllocated = false;
      for (int j = 0; j < super.num_data; j++)
      {
        int byteIndex = j / 8;
        int bitIndex = j % 8;

        if (!(dataBitmapBuffer[byteIndex] & (1 << bitIndex)))
        {
          dataBitmapBuffer[byteIndex] |= (1 << bitIndex);
          inode.direct[i] = super.data_region_addr + j;
          blockAllocated = true;
          break;
        }
      }

      if (!blockAllocated)
      {
        return -ENOTENOUGHSPACE;
      }
    }
  }

  disk->writeBlock(super.data_bitmap_addr, dataBitmapBuffer);

  // Write data to blocks
  int bytesWritten = 0;
  for (int i = 0; i < requiredBlocks; i++)
  {
    if (inode.direct[i] <= 0 ||
        static_cast<unsigned int>(inode.direct[i]) >= static_cast<unsigned int>(super.data_region_addr + super.num_data))
    {
      return -EINVALIDINODE;
    }

    int blockNum = inode.direct[i];
    int offset = bytesWritten;
    int chunkSize = std::min(UFS_BLOCK_SIZE, size - bytesWritten);

    char blockBuffer[UFS_BLOCK_SIZE] = {0};
    memcpy(blockBuffer, static_cast<const char *>(buffer) + offset, chunkSize);
    disk->writeBlock(blockNum, blockBuffer);

    bytesWritten += chunkSize;
  }

  inode.size = size;
  memcpy(inodeBlockBuffer + inodeOffset, &inode, sizeof(inode_t));
  disk->writeBlock(inodeBlock, inodeBlockBuffer);

  return bytesWritten;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name)
{
  // 1. Validate `name`
  if (name.empty() || name.length() >= DIR_ENT_NAME_SIZE)
  {
    return -EINVALIDNAME; // Invalid name
  }
  if (name == "." || name == "..")
  {
    return -EUNLINKNOTALLOWED; // Cannot unlink "." or ".."
  }

  // Read the superblock
  super_t super;
  char superBlockBuffer[UFS_BLOCK_SIZE];
  disk->readBlock(0, superBlockBuffer);
  memcpy(&super, superBlockBuffer, sizeof(super_t));

  // Validate `parentInodeNumber`
  if (parentInodeNumber < 0 || parentInodeNumber >= super.num_inodes)
  {
    return -EINVALIDINODE; // Out of bounds
  }

  // Read the inode bitmap
  unsigned char inodeBitmap[super.inode_bitmap_len * UFS_BLOCK_SIZE];
  for (int i = 0; i < super.inode_bitmap_len; i++)
  {
    disk->readBlock(super.inode_bitmap_addr + i, inodeBitmap + i * UFS_BLOCK_SIZE);
  }

  // Check if parent inode is allocated
  int byteOffset = parentInodeNumber % 8;
  int bitmapByte = parentInodeNumber / 8;
  char bitmask = 0b1 << byteOffset;
  if (!(inodeBitmap[bitmapByte] & bitmask))
  {
    return -EINVALIDINODE; // Not allocated
  }

  // Read parent inode
  int inodesPerBlock = UFS_BLOCK_SIZE / sizeof(inode_t);
  int parentInodeBlock = super.inode_region_addr + (parentInodeNumber / inodesPerBlock);
  int parentInodeOffset = (parentInodeNumber % inodesPerBlock) * sizeof(inode_t);
  char parentInodeBlockBuffer[UFS_BLOCK_SIZE];
  disk->readBlock(parentInodeBlock, parentInodeBlockBuffer);
  inode_t parentInode;
  memcpy(&parentInode, parentInodeBlockBuffer + parentInodeOffset, sizeof(inode_t));

  if (parentInode.type != UFS_DIRECTORY)
  {
    return -EINVALIDINODE; // Parent is not a directory
  }

  // 4. Locate the target entry in the parent directory
  int totalEntries = parentInode.size / sizeof(dir_ent_t);
  dir_ent_t dirEntries[totalEntries];
  read(parentInodeNumber, dirEntries, parentInode.size);

  int targetIndex = -1;
  for (int i = 0; i < totalEntries; i++)
  {
    if (name == dirEntries[i].name)
    {
      targetIndex = i;
      break;
    }
  }

  if (targetIndex == -1)
  {
    return -ENOTFOUND; // Entry not found
  }

  // Get the inode number of the target
  int targetInodeNumber = dirEntries[targetIndex].inum;

  // Read the target inode
  int targetInodeBlock = super.inode_region_addr + (targetInodeNumber / inodesPerBlock);
  int targetInodeOffset = (targetInodeNumber % inodesPerBlock) * sizeof(inode_t);
  char targetInodeBlockBuffer[UFS_BLOCK_SIZE];
  disk->readBlock(targetInodeBlock, targetInodeBlockBuffer);
  inode_t targetInode;
  memcpy(&targetInode, targetInodeBlockBuffer + targetInodeOffset, sizeof(inode_t));

  // 5. Handle unlinking based on type
  if (targetInode.type == UFS_DIRECTORY)
  {
    // Ensure directory is empty (only `.` and `..` should exist)
    if (targetInode.size > static_cast<int>(2 * sizeof(dir_ent_t)))
    {
      return -EDIRNOTEMPTY; // Cannot unlink a non-empty directory
    }
  }

  // Deallocate data blocks (if any)
  unsigned char dataBitmap[super.data_bitmap_len * UFS_BLOCK_SIZE];
  for (int i = 0; i < super.data_bitmap_len; i++)
  {
    disk->readBlock(super.data_bitmap_addr + i, dataBitmap + i * UFS_BLOCK_SIZE);
  }

  int numBlocks = (targetInode.size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;
  for (int i = 0; i < numBlocks; i++)
  {
    int blockNumber = targetInode.direct[i] - super.data_region_addr;
    int byteIndex = blockNumber / 8;
    int bitIndex = blockNumber % 8;
    dataBitmap[byteIndex] &= ~(1 << bitIndex); // Mark block as free
  }

  // Write updated data bitmap
  for (int i = 0; i < super.data_bitmap_len; i++)
  {
    disk->writeBlock(super.data_bitmap_addr + i, dataBitmap + i * UFS_BLOCK_SIZE);
  }

  // Deallocate the target inode
  int targetByteIndex = targetInodeNumber / 8;
  int targetBitIndex = targetInodeNumber % 8;
  inodeBitmap[targetByteIndex] &= ~(1 << targetBitIndex); // Mark inode as free

  // Write updated inode bitmap
  for (int i = 0; i < super.inode_bitmap_len; i++)
  {
    disk->writeBlock(super.inode_bitmap_addr + i, inodeBitmap + i * UFS_BLOCK_SIZE);
  }

  // 6. Remove the directory entry from the parent
  if (targetIndex < totalEntries - 1)
  {
    dirEntries[targetIndex] = dirEntries[totalEntries - 1]; // Replace with last entry
  }
  parentInode.size -= sizeof(dir_ent_t);

  // Write back the updated parent directory
  write(parentInodeNumber, dirEntries, parentInode.size);

  // Update parent inode
  memcpy(parentInodeBlockBuffer + parentInodeOffset, &parentInode, sizeof(inode_t));
  disk->writeBlock(parentInodeBlock, parentInodeBlockBuffer);

  return 0; // Success
}