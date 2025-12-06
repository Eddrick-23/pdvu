#pragma once
#include <string>

class Tempfile {
public:
    Tempfile();
    ~Tempfile();

    //delete copy constructors, ownership must be unique
    Tempfile(const Tempfile&) = delete;
    Tempfile& operator=(const Tempfile&) = delete;

    // move constructor
    Tempfile(Tempfile&& other) noexcept;
    Tempfile& operator=(Tempfile&& other) noexcept;

    void close_file();
    const std::string& path() const;
    bool write_data(const unsigned char* data, size_t len);
private:
    std::string fp;
    int fd;
};