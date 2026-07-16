#include "tempfile.h"

#include <unistd.h>

#include <cstring>
#include <iostream>

Tempfile::Tempfile(size_t size) {
  file_size = size;
  char tmp_template[] = "/tmp/pdvu_XXXXXX";
  fd = mkstemp(tmp_template);  // create actual file
  if (fd == -1) {
    throw std::runtime_error(std::string("mkstemp failed: ") + strerror(errno));
  }
  fp = std::string(tmp_template);  // store the file path

  if (ftruncate(fd, file_size) == -1) {  // set size
    close(fd);
    unlink(fp.c_str());
    throw std::runtime_error("Failed to set temp file size: " + fp);
  }

  // creat raw pointer to buffer
  mapped_ptr = mmap(nullptr, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapped_ptr == MAP_FAILED) {
    close(fd);
    unlink(fp.c_str());
    throw std::runtime_error("Failed to map temp file: " + fp);
  }
}

Tempfile::~Tempfile() {
  if (mapped_ptr != MAP_FAILED) {
    munmap(mapped_ptr, file_size);
  }
  close_file();
}

void Tempfile::close_file() {  // close and unlink
  if (fd != -1) {
    close(fd);
    fd = -1;  // mark as closed
  }
  if (!fp.empty()) {
    unlink(fp.c_str());
  }
}

const std::string& Tempfile::path() const {  // get path
  return fp;
}

void* Tempfile::data() const { return mapped_ptr; }

Tempfile::WriteStatus Tempfile::write_data(const unsigned char* data, size_t len) {
  if (data == nullptr) {
    return WriteStatus::NullBuffer;
  }
  if (mapped_ptr == MAP_FAILED) {
    return WriteStatus::UnmappedPointer;
  }
  if (len > file_size) {
    return WriteStatus::SizeExceeded;
  }
  memcpy(mapped_ptr, data, len);
  return WriteStatus::Success;
}

Tempfile::Tempfile(Tempfile&& other) noexcept {
  fp = std::move(other.fp);
  fd = other.fd;
  mapped_ptr = other.mapped_ptr;
  file_size = other.file_size;

  other.fd = -1;  // so other doesn't close the file when its destroyed
  other.fp.clear();
  other.mapped_ptr = MAP_FAILED;
  other.file_size = 0;
};

Tempfile& Tempfile::operator=(Tempfile&& other) noexcept {
  if (this != &other) {
    close_file();  // close and unlink current file

    fp = std::move(other.fp);
    fd = other.fd;
    mapped_ptr = other.mapped_ptr;
    file_size = other.file_size;

    other.fd = -1;
    other.fp.clear();
    other.mapped_ptr = MAP_FAILED;
    other.file_size = 0;
  }
  return *this;
}