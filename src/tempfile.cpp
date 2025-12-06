#include <iostream>
#include <unistd.h>
#include <__ostream/basic_ostream.h>
#include "tempfile.h"


Tempfile::Tempfile() {
    char tmp_template[] = "/tmp/pdvu_XXXXXX";
    fd = mkstemp(tmp_template); // create actual file
    if (fd == -1) {
        perror("mkstemp");
    }
    fp = std::string(tmp_template); // store the file path
}

Tempfile::~Tempfile() {
    close_file();
    if (!fp.empty()) {
        unlink(fp.c_str());
    }
}

bool Tempfile::write_data(const unsigned char* data, size_t len) {
    if (fp.empty()) {
        std::cerr << "Tempfile has not been created" << std::endl;
        return false;
    }

    // handle partial writes
    size_t total = 0;
    while (total < len) {
        ssize_t written = write(fd, data + total, len - total);
        if (written == -1) {
            perror("write");
            return false;
        }
        total += written;
    }
    return true;
}

void Tempfile::close_file() { // close after writing
   if (fd != -1) {
       close(fd);
       fd = -1; // mark as closed
   }
}

const std::string& Tempfile::path() const{ // get path
    return fp;
}

Tempfile::Tempfile(Tempfile&& other) noexcept {
    fp = std::move(other.fp);
    fd = other.fd;

    other.fd = -1; // so other doesnt close the file when its destroyed
    other.fp.clear();
};

Tempfile& Tempfile::operator=(Tempfile&& other) noexcept {
    if (this != &other) {
        close_file();

        fd = other.fd;
        fp = other.fp;

        other.fd = -1;
        other.fp.clear();
    }
    return *this;
}