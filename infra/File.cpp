#include "File.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
//--------------------------------------------------------------------------------
using namespace std;
using namespace infra;
namespace fs = std::filesystem;
//--------------------------------------------------------------------------------
namespace infra {
//--------------------------------------------------------------------------------
const char* getOSCachingName(File::OSCaching c) {
   using OSCaching = File::OSCaching;
   switch (c) {
    case OSCaching::True: return "True";
    case OSCaching::ODIRECT: return "False";
    case OSCaching::OSync: return "OSync";
   }
   unreachable();
}
//--------------------------------------------------------------------------------
void File::checkFileError() {
   if (fileDescriptor == invalidFd) {
      throw std::runtime_error("error opening file at `" + name.string() + "`: " + getErrorString());
   }
}
//--------------------------------------------------------------------------------
const char* File::getOpenModeName(OpenMode mode) {
   switch (mode) {
    case OpenMode::Overwrite: return "overwrite";
    case OpenMode::Create: return "create";
    case OpenMode::CreateIfNotExists: return "create-if-not-exists";
    case OpenMode::Open: return "open";
   }
   unreachable();
}
//--------------------------------------------------------------------------------
bool File::isBlockDevice(fs::path name) {
   return fs::is_block_file(name);
}
//--------------------------------------------------------------------------------
bool File::isDirectory(fs::path name) {
   return fs::is_directory(name);
}
//--------------------------------------------------------------------------------
File::File(fs::path name, AccessMode accessMode, OSCaching osCaching)
   : name{name}, accessMode{accessMode}, osCaching{osCaching}, blockDevice{isBlockDevice(name)} {
}
//--------------------------------------------------------------------------------
File::File(File&& other)
   : fileDescriptor{other.fileDescriptor}, name{std::move(other.name)}, accessMode{other.accessMode}, osCaching{other.osCaching}, blockDevice{other.blockDevice} {
   other.fileDescriptor = invalidFd;
}
//--------------------------------------------------------------------------------
File& File::operator=(File&& other)
{
   fileDescriptor = other.fileDescriptor;
   name = std::move(other.name);
   accessMode = other.accessMode;
   osCaching = other.osCaching;
   blockDevice = other.blockDevice;

   other.fileDescriptor = invalidFd;
   return *this;
}
//--------------------------------------------------------------------------------
string File::readWholeFile() {
   assert(isOpen());
   string result;
   char buffer[512];
   while (int readBytes = ::read(fileDescriptor, buffer, sizeof(buffer))) {
      result.append(buffer,readBytes);
   }
   return result;
}
//--------------------------------------------------------------------------------
bool File::isOpen() const {
   return fileDescriptor != invalidFd;
}
//--------------------------------------------------------------------------------
void File::close() {
   ::close(fileDescriptor);
   fileDescriptor = invalidFd;
}
//--------------------------------------------------------------------------------
File::~File() {
   if (isOpen()) {
      close();
   }
}
//--------------------------------------------------------------------------------
int getPosixAccessMode(File::AccessMode mode) {
   switch (mode) {
    case File::AccessMode::ReadOnly: return O_RDONLY;
    case File::AccessMode::WriteOnly: return O_WRONLY;
    case File::AccessMode::ReadWrite: return O_RDWR;
   }
   unreachable();
}
//--------------------------------------------------------------------------------
int getPosixOpenMode(OpenMode mode) {
   switch (mode) {
      case OpenMode::Overwrite: return O_CREAT | O_TRUNC; // Will always be successful and trunc file
      case OpenMode::Open: return 0; // Will fail if file does not exist
      case OpenMode::Create: return O_CREAT | O_EXCL; // Will fail if file exists
      case OpenMode::CreateIfNotExists: return O_CREAT; // Will always be successful and keep file
   }
   unreachable();
}
//--------------------------------------------------------------------------------
void File::open(OpenMode openMode) {
   // See https://stackoverflow.com/questions/5055859/how-are-the-o-sync-and-o-direct-flags-in-open2-different-alike
   auto oDirectFlag = (osCaching != OSCaching::True) ? O_DIRECT : 0;
   auto dSyncFlag = (osCaching == OSCaching::OSync) ? O_DSYNC : 0;

   // ReadOnly --> Open
   assert((accessMode != AccessMode::ReadOnly) || (openMode == OpenMode::Open));

   fileDescriptor = ::open(name.c_str(), getPosixAccessMode(accessMode) | getPosixOpenMode(openMode) | oDirectFlag | dSyncFlag, S_IRUSR | S_IWUSR);
   checkFileError();
}
//--------------------------------------------------------------------------------
File& File::operator<<(string_view sv)
   // Stream into a file
{
   write(sv.data(),sv.size());
   return *this;
}
//--------------------------------------------------------------------------------
void File::datasync()
{
   auto result = fdatasync(fileDescriptor);
   if (result == invalidFd) {
      throw std::runtime_error("error datasyncing file " + name.string() + ": " + getErrorString());
   }
}
//--------------------------------------------------------------------------------
void File::erase()
{
   assert(!isOpen());
   if (::remove(name.c_str()) == invalidFd) {
      throw std::runtime_error("error removing file " + name.string() + ": " + getErrorString());
   }
}
//--------------------------------------------------------------------------------
void File::move(string_view newPath)
{
   assert(!isOpen());
   if (::rename(name.c_str(),string{newPath}.c_str()) == invalidFd) {
      throw std::runtime_error("error moving file " + name.string() + " to new location `" + string{newPath} + "`:" + getErrorString());
   }
   name = newPath;
}
//--------------------------------------------------------------------------------
uint64_t File::size() const {
   if (blockDevice) {
      uint64_t size;
      if (ioctl(fileDescriptor, BLKGETSIZE64, &size) == invalidFd) {
         throw std::runtime_error("error getting size of block device " + name.string() + ": " + getErrorString());
      }
      return size;
   } else {
      struct stat statbuf;
      if (stat(name.c_str(), &statbuf) == invalidFd) {
         throw std::runtime_error("error getting size of file " + name.string() + ": " + getErrorString());
      }
      return statbuf.st_size;
   }
}
//--------------------------------------------------------------------------------
void File::read(void* destination, uint64_t size, size_t offset) {
   assert(isOpen());
   assert((osCaching != OSCaching::ODIRECT) || ((reinterpret_cast<uintptr_t>(destination) % 512) == 0));
   auto readBytes = pread(fileDescriptor,destination,size,offset);
   if (readBytes == invalidFd) {
      throw std::runtime_error("error reading from file " + name.string() + ": " + getErrorString());
   }
   if (readBytes != static_cast<long>(size)) {
      throw std::runtime_error("error reading from file (incomplete read): " + name.string());
   }
}
//--------------------------------------------------------------------------------
void File::write(const void* source,uint64_t size,size_t offset) {
   assert(isOpen());
   auto writtenBytes = pwrite(fileDescriptor,source,size,offset);
   if (writtenBytes == invalidFd) {
      throw std::runtime_error("error writing to file " + name.string() + ": " + getErrorString());
   }
   if (writtenBytes != static_cast<long>(size)) {
      throw std::runtime_error("error writing to file " + name.string());
   }
}
//--------------------------------------------------------------------------------
void File::write(const void* source,uint64_t size) {
   assert(isOpen());
   auto writtenBytes = ::write(fileDescriptor,source,size);
   if (writtenBytes == invalidFd) {
      throw std::runtime_error("error writing to file " + name.string() + ": " + getErrorString());
   }
   if (writtenBytes != static_cast<long>(size)) {
      throw std::runtime_error("error appending to file " + name.string());
   }
}
//--------------------------------------------------------------------------------
string File::generateUniqueFileName(const string& prefix) {
   return prefix + to_string(chrono::time_point_cast<chrono::seconds>(chrono::high_resolution_clock::now()).time_since_epoch().count());
}
//--------------------------------------------------------------------------------
void File::reserveSpace(size_t size) {
   if (!blockDevice) {
      posix_fallocate(fileDescriptor, 0, size);
   }
}
//--------------------------------------------------------------------------------
}
//--------------------------------------------------------------------------------
