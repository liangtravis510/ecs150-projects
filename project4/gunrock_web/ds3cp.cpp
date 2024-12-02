#include <iostream>
#include <string>
#include <memory>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

int main(int argc, char *argv[])
{
  if (argc != 4)
  {
    cerr << argv[0] << ": diskImageFile src_file dst_inode" << endl;
    cerr << "For example:" << endl;
    cerr << "    $ " << argv[0] << " tests/disk_images/a.img dthread.cpp 3" << endl;
    return 1;
  }

  // Parse command line arguments
  /*
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  string srcFile = string(argv[2]);
  int dstInode = stoi(argv[3]);
  */
  unique_ptr<Disk> disk = make_unique<Disk>(argv[1], UFS_BLOCK_SIZE);
  unique_ptr<LocalFileSystem> fileSystem = make_unique<LocalFileSystem>(disk.get());
  string srcFile = string(argv[2]);
  int dstInode = stoi(argv[3]);

  // Open the source file
  int srcFd = open(srcFile.c_str(), O_RDONLY);
  if (srcFd < 0)
  {
    cerr << "Failed to open source file: " << srcFile << endl;
    return 1;
  }

  // Read and write the file in chunks
  char buffer[UFS_BLOCK_SIZE];
  int bytesRead = 0;
  int totalBytesWritten = 0;

  disk->beginTransaction(); // Start transaction
  while ((bytesRead = read(srcFd, buffer, UFS_BLOCK_SIZE)) > 0)
  {
    int result = fileSystem->write(dstInode, buffer, bytesRead);
    if (result < 0)
    {
      disk->rollback(); // Rollback if an error occurs
      cerr << "Write error for inode " << dstInode << ": " << result << endl;
      close(srcFd);
      return 1;
    }
    totalBytesWritten += bytesRead;
  }

  if (bytesRead < 0)
  {
    disk->rollback();
    cerr << "Read error for source file: " << srcFile << endl;
    close(srcFd);
    return 1;
  }

  disk->commit(); // Commit transaction
  close(srcFd);

  // cout << "Copied " << totalBytesWritten << " bytes from " << srcFile << " to inode " << dstInode << endl;
  return 0;
}
