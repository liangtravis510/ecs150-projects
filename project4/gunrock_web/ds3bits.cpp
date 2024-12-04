#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>
#include <memory>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    cerr << argv[0] << ": diskImageFile" << endl;
    return 1;
  }

  // Parse command line arguments
  /*
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  */

  unique_ptr<Disk> disk = make_unique<Disk>(argv[1], UFS_BLOCK_SIZE);
  unique_ptr<LocalFileSystem> fileSystem = make_unique<LocalFileSystem>(disk.get());

  // Get metadata
  super_t super;
  fileSystem->readSuperBlock(&super);
  unsigned char inode_bitmap[super.inode_region_len * UFS_BLOCK_SIZE];
  fileSystem->readInodeBitmap(&super, inode_bitmap);
  unsigned char data_bitmap[super.data_region_len * UFS_BLOCK_SIZE];
  fileSystem->readDataBitmap(&super, data_bitmap);

  // Print filesystem metadata
  cout << "Super" << endl;
  cout << "inode_region_addr " << super.inode_region_addr << endl;
  cout << "inode_region_len " << super.inode_region_len << endl;
  cout << "num_inodes " << super.num_inodes << endl;
  cout << "data_region_addr " << super.data_region_addr << endl;
  cout << "data_region_len " << super.data_region_len << endl;
  cout << "num_data " << super.num_data << endl;
  cout << endl;

  // Print inode and data bitmaps
  cout << "Inode bitmap" << endl;
  int numBytes = super.num_inodes / 8;
  if (super.num_inodes % 8)
    numBytes++;
  for (int idx = 0; idx < numBytes; idx++)
    cout << static_cast<unsigned int>(inode_bitmap[idx]) << ' ';

  cout << endl
       << endl
       << "Data bitmap" << endl;
  numBytes = super.num_data / 8;
  if (super.num_data % 8 != 0)
    numBytes += 1;
  for (int idx = 0; idx < numBytes; idx++)
    cout << static_cast<unsigned int>(data_bitmap[idx]) << ' ';
  cout << endl;

  return 0;
}
