#pragma once
#include <string>
#include <sys/mman.h>
class Tempfile {
public:
    explicit Tempfile(size_t size);
    ~Tempfile();

    //delete copy constructors, ownership must be unique
    Tempfile(const Tempfile&) = delete;
    Tempfile& operator=(const Tempfile&) = delete;

    // move constructor
    Tempfile(Tempfile&& other) noexcept;
    Tempfile& operator=(Tempfile&& other) noexcept;

    void close_file();
    const std::string& path() const;
    void* data() const;
    void write_data(unsigned char* data, size_t len);
private:
    std::string fp;
    int fd;
    void* mapped_ptr = MAP_FAILED;
    size_t file_size;
};