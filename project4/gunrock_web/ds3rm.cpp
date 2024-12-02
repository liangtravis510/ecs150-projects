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
  if (argc != 4)
  {
    cerr << argv[0] << ": diskImageFile parentInode entryName" << endl;
    return 1;
  }

  // Parse command line arguments
  /*
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  int parentInode = stoi(argv[2]);
  string entryName = string(argv[3]);
  */

  unique_ptr<Disk> disk = make_unique<Disk>(argv[1], UFS_BLOCK_SIZE);
  unique_ptr<LocalFileSystem> fileSystem = make_unique<LocalFileSystem>(disk.get());
  int parentInode = stoi(argv[2]);
  string entryName = string(argv[3]);

  disk->beginTransaction();
  if (fileSystem->unlink(parentInode, entryName) < 0)
  {
    disk->rollback();
    cerr << "Error removing entry" << endl;
    return 1;
  }
  disk->commit();
  return 0;
}
