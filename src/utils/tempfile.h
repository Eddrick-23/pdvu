#pragma once
#include <sys/mman.h>

#include <string>

/**
 * @breif An RAII wrapper for managing temporary memory-mapped files.
 *
 * This class handles the creation, sizing, mapping, and cleanup of a
 * temporary file. It guarantees unique ownership of the underlying
 * file descriptor and memory mapping.
 */
class Tempfile {
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
   * @brief Constructs a new Tempfile and allocates the specified size.
   *
   * Creates a temporary file in `/tmp/`, resizes it to the requested size,
   * and maps it into user space.
   *
   * @param size The size in bytes to allocate for the temporary file.
   * @throw std::runtime_error If file creation, sizing, or mapping fails.
   */
  explicit Tempfile(size_t size);

  /**
   * @brief Destroys the Tempfile, unmapping memory and removing the temporary file.
   */
  ~Tempfile();

  // delete copy constructors to enforce unique ownership of file resource.
  Tempfile(const Tempfile&) = delete;
  Tempfile& operator=(const Tempfile&) = delete;

  /**
   * @brief Move constructor. Transfers ownership of the temporary file resource.
   * @param other The Tempfile object to move from.
   */
  Tempfile(Tempfile&& other) noexcept;

  /**
   * @brief Move assignment operator. Transfers ownership of the temporary file resource.
   * @param other The Tempfile object to move from.
   * @return A reference to this Tempfile object.
   */
  Tempfile& operator=(Tempfile&& other) noexcept;

  /**
   * @brief Manually closes the file descriptor and unlinks the file path.
   */
  void close_file();

  /**
   * @brief Retrieves the filesystem path of the temporary file.
   * @return A constant reference to the file path string.
   */
  [[nodiscard]] const std::string& path() const;

  /**
   * @brief Retrieves a pointer to the memory-mapped temporary file.
   * @return A void pointer to the start of the memory mapping. Returns MAP_FAILED if unmapped.
   */
  [[nodiscard]] void* data() const;

  /**
   * @brief Writes data into the memory-mapped temporary file.
   * @param data Pointer to the source data buffer to write.
   * @param len The number of bytes to write.
   * @return A WriteStatus indicating success or the specific error encountered.
   */
  WriteStatus write_data(const unsigned char* data, size_t len);

 private:
  int fd;                         ///< File descriptor for the temporary file
  void* mapped_ptr = MAP_FAILED;  ///< Pointer to the mapped userspace memory
  size_t file_size;               ///< Allocated size of the file in bytes
  std::string fp;                 ///< Filesystem path of the temporary file
};