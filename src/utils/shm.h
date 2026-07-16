#pragma once
#include <sys/mman.h>

#include <string>

class SharedMemory {
 public:
  enum class WriteStatus {
    Success = 0,
    NullBuffer,
    UnmappedPointer,
    SizeExceeded,
  };

  static const char* to_string(const WriteStatus&);

  explicit SharedMemory(size_t image_size);
  ~SharedMemory();

  // delete copy constructors, ownership must be unique
  SharedMemory(const SharedMemory&) = delete;
  SharedMemory& operator=(const SharedMemory&) = delete;

  // move constructor
  SharedMemory(SharedMemory&& other) noexcept;
  SharedMemory& operator=(SharedMemory&& other) noexcept;

  const std::string& name() const;
  const size_t& size() const;
  void* data() const;
  WriteStatus write_data(const unsigned char* data, size_t len);
  void copy_data(void* dest, size_t len) const;

  void close_mem();

 private:
  int shm_fd = -1;  // shared memory file descriptor
  void* mapped_ptr = MAP_FAILED;
  size_t shm_size = 0;
  std::string shm_name;
};

int is_shm_supported();