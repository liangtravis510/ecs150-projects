
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

  // Check if the parent inode is a directory
  if (parentInode.type != UFS_DIRECTORY)
  {
    return -EINVALIDINODE;
  }

  // Validate directory size alignment
  if (parentInode.size % sizeof(dir_ent_t) != 0)
  {
    return -EINVALIDINODE;
  }

  // Read the contents of the directory
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
    if (entry->inum != -1 && name == string(entry->name))
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
  readSuperBlock(&super);

  // Validate the inode number
  if (inodeNumber < 0 || inodeNumber >= super.num_inodes)
  {
    return -EINVALIDINODE; // Invalid inode number
  }

  // Read the inode bitmap
  unsigned char inode_bitmap[super.inode_bitmap_len * UFS_BLOCK_SIZE];
  readInodeBitmap(&super, inode_bitmap);

  // Calculate the bit position for the inode in the bitmap
  int byte_offset = inodeNumber % 8; // Bit offset within the byte
  int bitmap_byte = inodeNumber / 8; // Byte index in the bitmap
  char bitmask = 0b1 << byte_offset;

  // Check if the inode exits and is allocated
  if (!(inode_bitmap[bitmap_byte] & bitmask))
  {
    return -ENOTALLOCATED;
  }

  // Read the inode region
  inode_t inodes[super.inode_region_len * UFS_BLOCK_SIZE / sizeof(inode_t)];
  readInodeRegion(&super, inodes);

  // Return the inode data
  *inode = inodes[inodeNumber];
  return 0;
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size)
{
  // Invalid size
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

  // Check if parent is a directory
  if (parentInode.type != UFS_DIRECTORY)
  {
    return -EINVALIDINODE;
  }

  // Check if name is valid
  if (name.length() >= DIR_ENT_NAME_SIZE)
  {
    return -EINVALIDNAME;
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

  // Initialize new inode
  inode_t newInode = {};
  newInode.type = type;
  newInode.size = (type == UFS_DIRECTORY) ? 2 * sizeof(dir_ent_t) : 0;

  // If creating a directory, initialize `.` and `..`
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
      // No free blocks available, rollback inode allocation
      int byte_offset = newInodeNumber % 8;
      int bitmap_byte = newInodeNumber / 8;
      inodeBitmap[bitmap_byte] &= ~(0b1 << byte_offset); // Free the inode
      writeInodeBitmap(&super, inodeBitmap);

      return -ENOTENOUGHSPACE; // No free blocks available
    }

    writeDataBitmap(&super, dataBitmap);

    // Initialize directory entries
    dir_ent_t entries[2];
    strncpy(entries[0].name, ".", DIR_ENT_NAME_SIZE - 1);
    entries[0].name[DIR_ENT_NAME_SIZE - 1] = '\0'; // Null-terminate
    entries[0].inum = newInodeNumber;              // "." points to itself

    strncpy(entries[1].name, "..", DIR_ENT_NAME_SIZE - 1);
    entries[1].name[DIR_ENT_NAME_SIZE - 1] = '\0'; // Null-terminate
    entries[1].inum = parentInodeNumber;           // ".." points to the parent directory

    // Write entries to the new directory block
    char newDirBlock[UFS_BLOCK_SIZE] = {0};
    memcpy(newDirBlock, entries, sizeof(entries));
    disk->writeBlock(super.data_region_addr + freeBlock, newDirBlock);

    newInode.direct[0] = super.data_region_addr + freeBlock;
  }

  // Update inode region with the new inode
  inode_t inodes[super.inode_region_len * UFS_BLOCK_SIZE / sizeof(inode_t)];
  readInodeRegion(&super, inodes);
  inodes[newInodeNumber] = newInode;
  writeInodeRegion(&super, inodes);

  // Add the entry to the parent directory
  dir_ent_t newEntry = {};
  strncpy(newEntry.name, name.c_str(), DIR_ENT_NAME_SIZE - 1);
  newEntry.name[DIR_ENT_NAME_SIZE - 1] = '\0'; // Null-terminate
  newEntry.inum = newInodeNumber;

  char parentDirBlock[UFS_BLOCK_SIZE];
  disk->readBlock(parentInode.direct[0], parentDirBlock);

  memcpy(parentDirBlock + parentInode.size, &newEntry, sizeof(newEntry));
  disk->writeBlock(parentInode.direct[0], parentDirBlock);

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

  // Load superblock and validate inodeNumber
  super_t super;
  readSuperBlock(&super);

  if (inodeNumber < 0 || inodeNumber >= super.num_inodes)
  {
    return -EINVALIDINODE;
  }

  // Load inode bitmap and validate allocation
  unsigned char inode_bitmap[super.inode_bitmap_len * UFS_BLOCK_SIZE];
  readInodeBitmap(&super, inode_bitmap);

  int byte_offset = inodeNumber % 8;
  int bitmap_byte = inodeNumber / 8;
  if (!(inode_bitmap[bitmap_byte] & (0b1 << byte_offset)))
  {
    return -ENOTALLOCATED;
  }

  // Load inodes and check type
  inode_t inodes[super.inode_region_len * UFS_BLOCK_SIZE / sizeof(inode_t)];
  readInodeRegion(&super, inodes);
  inode_t &inode = inodes[inodeNumber];

  if (inode.type == UFS_DIRECTORY)
  {
    return -EWRITETODIR;
  }

  // Calculate current and required blocks
  int current_blocks = (inode.size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;
  int required_blocks = (size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;

  if (required_blocks > DIRECT_PTRS)
  {
    return -EINVALIDSIZE; // Exceeds maximum file size
  }

  // Load data bitmap
  unsigned char data_bitmap[super.data_bitmap_len * UFS_BLOCK_SIZE];
  readDataBitmap(&super, data_bitmap);

  // Allocate additional blocks if needed
  for (int i = current_blocks; i < required_blocks; ++i)
  {
    int free_block = -1;
    for (int j = 0; j < super.num_data; ++j)
    {
      int byte_idx = j / 8;
      int bit_idx = j % 8;
      if (!(data_bitmap[byte_idx] & (1 << bit_idx)))
      {
        free_block = j;
        data_bitmap[byte_idx] |= (1 << bit_idx);
        break;
      }
    }
    if (free_block == -1)
    {
      return -ENOTENOUGHSPACE; // Not enough space
    }
    inode.direct[i] = super.data_region_addr + free_block;
  }

  // Deallocate unused blocks if reducing size
  for (int i = required_blocks; i < current_blocks; ++i)
  {
    int relative_block = inode.direct[i] - super.data_region_addr;
    int byte_idx = relative_block / 8;
    int bit_idx = relative_block % 8;
    data_bitmap[byte_idx] &= ~(0b1 << bit_idx);
    inode.direct[i] = 0;
  }

  // Write updated data bitmap
  writeDataBitmap(&super, data_bitmap);

  // Write data to allocated blocks
  const char *data_ptr = static_cast<const char *>(buffer);
  int bytes_written = 0;

  for (int i = 0; i < required_blocks; ++i)
  {
    char block_data[UFS_BLOCK_SIZE] = {0};
    int bytes_to_write = min(size - bytes_written, UFS_BLOCK_SIZE);
    memcpy(block_data, data_ptr, bytes_to_write);
    disk->writeBlock(inode.direct[i], block_data);
    data_ptr += bytes_to_write;
    bytes_written += bytes_to_write;
  }

  // Update inode size
  inode.size = size;
  writeInodeRegion(&super, inodes);

  return bytes_written;
}

int LocalFileSystem::unlink(int parentInodeNumber, std::string name)
{
  // Read super block
  super_t super;
  readSuperBlock(&super);

  // Validate parent inode
  if (parentInodeNumber < 0 || static_cast<unsigned int>(parentInodeNumber) >= static_cast<unsigned int>(super.num_inodes))
  {
    return -EINVALIDINODE;
  }

  inode_t parentInode;
  this->stat(parentInodeNumber, &parentInode);

  // Check if the parent inode is a directory
  if (parentInode.type != UFS_DIRECTORY)
  {
    return -EINVALIDINODE;
  }

  // Validate parentInode.direct[0]
  if (static_cast<unsigned int>(parentInode.direct[0]) < static_cast<unsigned int>(super.data_region_addr) ||
      static_cast<unsigned int>(parentInode.direct[0]) >= static_cast<unsigned int>(super.data_region_addr + super.num_data))
  {
    return -EINVALIDINODE;
  }

  // Check for special cases: '.' and '..'
  if (name == "." || name == "..")
  {
    return -EUNLINKNOTALLOWED;
  }

  // Read the directory block
  char dirBlock[UFS_BLOCK_SIZE] = {0};
  disk->readBlock(parentInode.direct[0], dirBlock);

  // Find the entry to unlink
  bool entryFound = false;
  int entryIndex = -1;
  dir_ent_t *entries = reinterpret_cast<dir_ent_t *>(dirBlock);
  for (unsigned int i = 0; i < static_cast<unsigned int>(parentInode.size / sizeof(dir_ent_t)); ++i)
  {
    if (strncmp(entries[i].name, name.c_str(), DIR_ENT_NAME_SIZE) == 0)
    {
      entryFound = true;
      entryIndex = static_cast<int>(i);
      break;
    }
  }

  if (!entryFound)
  {
    return 0;
  }

  // Check if the entry is a directory and not empty
  inode_t targetInode;
  this->stat(entries[entryIndex].inum, &targetInode);
  if (targetInode.type == UFS_DIRECTORY &&
      static_cast<unsigned int>(targetInode.size) > static_cast<unsigned int>(2 * sizeof(dir_ent_t)))
  {
    return -EDIRNOTEMPTY; // Cannot remove a non-empty directory
  }

  // Mark the inode as free in the inode bitmap
  unsigned char inodeBitmap[super.inode_bitmap_len * UFS_BLOCK_SIZE];
  readInodeBitmap(&super, inodeBitmap);

  int inodeIndex = entries[entryIndex].inum;
  int byteIndex = inodeIndex / 8;
  int bitOffset = inodeIndex % 8;
  inodeBitmap[byteIndex] &= ~(1 << bitOffset);
  writeInodeBitmap(&super, inodeBitmap);

  // Clear data blocks if the entry is a file
  if (targetInode.type != UFS_DIRECTORY)
  {
    unsigned char dataBitmap[super.data_bitmap_len * UFS_BLOCK_SIZE];
    readDataBitmap(&super, dataBitmap);

    for (int i = 0; i < DIRECT_PTRS && targetInode.direct[i] != 0; ++i)
    {
      int dataIndex = targetInode.direct[i] - super.data_region_addr;
      int byteIdx = dataIndex / 8;
      int bitIdx = dataIndex % 8;
      dataBitmap[byteIdx] &= ~(1 << bitIdx);
    }
    writeDataBitmap(&super, dataBitmap);
  }

  // Clear data blocks if entry is a directory
  if (targetInode.type == UFS_DIRECTORY)
  {
    unsigned char dataBitmap[super.data_bitmap_len * UFS_BLOCK_SIZE];
    readDataBitmap(&super, dataBitmap);
    for (int i = 0; i < DIRECT_PTRS && targetInode.direct[i] != 0; ++i)
    {
      int dataIndex = targetInode.direct[i] - super.data_region_addr;
      int byteIndex = dataIndex / 8;
      int bitOffset = dataIndex % 8;
      dataBitmap[byteIndex] &= ~(1 << bitOffset);
    }
    writeDataBitmap(&super, dataBitmap);
  }

  // Remove the entry by shifting subsequent entries left
  for (unsigned int i = static_cast<unsigned int>(entryIndex);
       i < static_cast<unsigned int>(parentInode.size / sizeof(dir_ent_t)) - 1; ++i)
  {
    entries[i] = entries[i + 1];
  }
  parentInode.size -= static_cast<unsigned int>(sizeof(dir_ent_t));
  memset(&entries[parentInode.size / sizeof(dir_ent_t)], 0, sizeof(dir_ent_t));

  // Write updated directory block
  disk->writeBlock(parentInode.direct[0], dirBlock);

  // Update parent inode size
  inode_t inodes[super.inode_region_len * UFS_BLOCK_SIZE / sizeof(inode_t)];
  readInodeRegion(&super, inodes);
  inodes[parentInodeNumber] = parentInode;
  writeInodeRegion(&super, inodes);

  return 0;
}