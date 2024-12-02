#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <memory>

#include "StringUtils.h"
#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

// Use this function with std::sort for directory entries
bool compareByName(const dir_ent_t &a, const dir_ent_t &b)
{
  return std::strcmp(a.name, b.name) < 0;
}

int listDirectory(LocalFileSystem *fileSystem, const string &directory)
{
  if (directory.empty() || directory[0] != '/')
  {
    cerr << "Directory not found" << endl;
    return 1;
  }

  // Find inode of directory
  int inode_num = UFS_ROOT_DIRECTORY_INODE_NUMBER;
  int parent_inode_num = inode_num;
  if (directory.size() != 1)
  {
    string segment;
    for (auto iter = directory.begin() + 1; iter != directory.end(); ++iter)
    {
      if (*iter == '/')
      {
        parent_inode_num = inode_num;
        inode_num = fileSystem->lookup(inode_num, segment);
        if (inode_num < 0)
        {
          cerr << "Directory not found" << endl;
          return 1;
        }
        segment.clear();
      }
      else
      {
        segment += *iter;
      }
    }
    // One more time for last path segment
    parent_inode_num = inode_num;
    inode_num = fileSystem->lookup(inode_num, segment);
    if (inode_num < 0)
    {
      cerr << "Directory not found" << endl;
      return 1;
    }
  }

  // Create inode struct
  inode_t inode;
  if (fileSystem->stat(inode_num, &inode))
  {
    cerr << "Directory not found" << endl;
    return 1;
  }

  // List directory
  if (inode.type == UFS_REGULAR_FILE)
  {
    // Create parent inode
    inode_t parent_inode;
    if (fileSystem->stat(parent_inode_num, &parent_inode))
    {
      cerr << "Directory not found" << endl;
      return 1;
    }
    // Get parent inode directory entries
    int num_entries = parent_inode.size / sizeof(dir_ent_t);
    vector<dir_ent_t> buffer(num_entries);
    int bytes_read = fileSystem->read(parent_inode_num, buffer.data(), parent_inode.size);
    if (bytes_read != parent_inode.size)
    {
      cerr << "Directory not found" << endl;
      return 1;
    }
    // Search for inode entry to get filename
    for (const auto &entry : buffer)
    {
      if (entry.inum == inode_num)
      {
        cout << inode_num << '\t' << entry.name << endl;
        return 0;
      }
    }
  }
  else
  {
    // Get directory entries
    int num_entries = inode.size / sizeof(dir_ent_t);
    vector<dir_ent_t> buffer(num_entries);
    int bytes_read = fileSystem->read(inode_num, buffer.data(), inode.size);
    if (bytes_read != inode.size)
    {
      cerr << "Directory not found" << endl;
      return 1;
    }

    // Print sorted list
    sort(buffer.begin(), buffer.end(), compareByName);
    for (const auto &entry : buffer)
    {
      cout << entry.inum << '\t' << entry.name << endl;
    }
  }
  return 0;
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    cerr << argv[0] << ": diskImageFile directory" << endl;
    cerr << "For example:" << endl;
    cerr << "    $ " << argv[0] << " tests/disk_images/a.img /a/b" << endl;
    return 1;
  }

  // parse command line arguments
  /*
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  string directory = string(argv[2]);
  */
  unique_ptr<Disk> disk = make_unique<Disk>(argv[1], UFS_BLOCK_SIZE);
  unique_ptr<LocalFileSystem> fileSystem = make_unique<LocalFileSystem>(disk.get());
  string directory = string(argv[2]);

  return listDirectory(fileSystem.get(), directory);
  return 0;
}