#pragma once
#include <sys/mman.h>

#include <string>

/**
 * @brief An RAII wrapper for managing POSIX shared memory objects.
 *
 * This class handles the creation, sizing, mapping, and cleanup of a
 * POSIX shared memory segment. It ensures exclusive ownership of the
 * underlying memory and file descriptor.
 */
class SharedMemory {
 public:
  /**
   * @brief Represents the outcome of a write operation to the shared memory.
   */
  enum class WriteStatus {
    Success = 0,      ///< Write completed successfully
    NullBuffer,       ///< The provided source data pointer was null.
    UnmappedPointer,  ///< The shared memory segment is not currently mapped.
    SizeExceeded,     ///< The requested write length exceeds the allocated memory size.
  };

  /**
   * @brief Converts a WriteStatus enum value to a human-readable string.
   * @param status The WriteStatus to convert.
   * @return A null-terminated string describing the status.
   */
  static const char* to_string(const WriteStatus& status);

  /**
   * @brief Constructs a new SharedMemory object and allocates the specified size.
   *
   * This generates a unique POSIX shared memory name, creates the object,
   * truncates it to the requested size, and maps it into user space.
   *
   * @param image_size The size in bytes to allocate for the shared memory segment.
   * @throw std::runtime_error If creating, sizing, or mapping the shared memory fails.
   */
  explicit SharedMemory(size_t image_size);

  /**
   * @brief Destroys the SharedMemory object, unmapping and unlinking the memory.
   */
  ~SharedMemory();

  // Delete copy constructors to enforce unique ownership of the memory segment
  SharedMemory(const SharedMemory&) = delete;
  SharedMemory& operator=(const SharedMemory&) = delete;

  /**
   * @brief Move constructor. Transfers ownership of the shared memory from another object.
   * @param other The SharedMemory object to move from.
   */
  SharedMemory(SharedMemory&& other) noexcept;

  /**
   * @brief Move assignment operator. Transfers ownership of the shared memory.
   * @param other The SharedMemory object to move from.
   * @return A reference to this SharedMemory object.
   */
  SharedMemory& operator=(SharedMemory&& other) noexcept;

  /**
   * @brief Retrieves the generated POSIX name of the shared memory object.
   * @return A constant reference to the name string.
   */
  const std::string& name() const;

  /**
   * @brief Retrieves the allocated size of the shared memory segment.
   * @return A constant reference to the size in bytes.
   */
  const size_t& size() const;

  /**
   * @brief Retrieves a pointer to the mapped shared memory.
   * @return A void pointer to the start of the memory segment. Returns MAP_FAILED if unmapped.
   */
  void* data() const;

  /**
   * @brief Writes data into the shared memory segment.
   * @param data Pointer to the source data buffer to write.
   * @param len The number of bytes to write.
   * @return A WriteStatus indicating success or the specific error encountered.
   */
  WriteStatus write_data(const unsigned char* data, size_t len);

  /**
   * @brief Copies data from the shared memory segment into a destination buffer.
   * @param dest Pointer to the destination buffer.
   * @param len The number of bytes to copy. Must not exceed the shared memory size.
   */
  void copy_data(void* dest, size_t len) const;

  /**
   * @brief Manually closes the underlying shared memory file descriptor.
   *
   * Note: This does not unmap the memory or unlink the shared memory object.
   */
  void close_mem();

 private:
  int shm_fd = -1;  ///< shared memory file descriptor.
  void* mapped_ptr = MAP_FAILED; ///< Pointer to the mapped userspace memory.
  size_t shm_size = 0; ///< Size of shared memory segment in bytes.
  std::string shm_name; ///< Unique POSIX name for the shared memory object.
};

/**
 * @brief Checks if the host operating system supports POSIX shared memory.
 *
 * This is tested by attempting to create and immediately unlink a dummy
 * shared memory object.
 *
 * @return 1 if POSIX shared memory is supported, 0 otherwise.
 */
int is_shm_supported();