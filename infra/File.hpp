#pragma once
//--------------------------------------------------------------------------------
#include "Config.hpp"
#include <cassert>
#include <fstream>
#include <filesystem>
#include <string_view>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
//--------------------------------------------------------------------------------
namespace infra {
//--------------------------------------------------------------------------------
enum class OpenMode { Create,
                      CreateIfNotExists,
                      Open,
                      Overwrite };
//--------------------------------------------------------------------------------
class File {
   public:
   enum class OSCaching { True,
                          ODIRECT, // Only O_DIRECT
                          OSync };
   enum class AccessMode { ReadOnly, WriteOnly, ReadWrite };
   protected:
   int fileDescriptor = invalidFd;
   std::filesystem::path name;
   AccessMode accessMode;
   OSCaching osCaching;
   bool blockDevice;

   static constexpr int invalidFd = -1;

   public:
   File() = default;
   File(std::filesystem::path name, AccessMode accessMode = AccessMode::ReadWrite, OSCaching osCaching = OSCaching::True);
   File(const File& other) = delete;
   File(File&& other);
   File& operator=(File&& other);
   ~File();

   static bool isBlockDevice(std::filesystem::path name);
   static bool isDirectory(std::filesystem::path name);

   static const char* getOpenModeName(OpenMode mode);
   void close();
   bool isOpen() const;
   void open(OpenMode openMode);
      //   void openCreate();
   std::string getFileName() const { return name; }
   void reserveSpace(size_t size);
   bool isBlockDevice() const { return blockDevice; }
   bool exists() const {
      std::ifstream infile(name);
      return infile.good();
   }
   void erase();
   void move(std::string_view newPath);
   uint64_t size() const;
   /// Read/Write
   void read(void* destination, uint64_t size, size_t offset);
   std::string readWholeFile();
   void write(const void* source, uint64_t size, size_t offset);
   void write(const void* source, uint64_t size);
   File& operator<<(std::string_view sv);
   void datasync();

   /// Getters
   int getFd() const { assert(isOpen()); return fileDescriptor; }
   AccessMode getAccessMode() const { return accessMode; }

   static std::string generateUniqueFileName(const std::string& prefix);
   protected:
   void checkFileError();
};
//--------------------------------------------------------------------------------
}
//--------------------------------------------------------------------------------
