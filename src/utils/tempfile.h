#pragma once
#include <sys/mman.h>

#include <string>
class Tempfile {
 public:
  enum class WriteStatus {
    Success = 0,
    NullBuffer,
    UnmappedPointer,
    SizeExceeded,
  };

  explicit Tempfile(size_t size);
  ~Tempfile();

  // delete copy constructors, ownership must be unique
  Tempfile(const Tempfile&) = delete;
  Tempfile& operator=(const Tempfile&) = delete;

  // move constructor
  Tempfile(Tempfile&& other) noexcept;
  Tempfile& operator=(Tempfile&& other) noexcept;

  void close_file();
  const std::string& path() const;
  void* data() const;
  WriteStatus write_data(const unsigned char* data, size_t len);

 private:
  std::string fp;
  int fd;
  void* mapped_ptr = MAP_FAILED;
  size_t file_size;
};