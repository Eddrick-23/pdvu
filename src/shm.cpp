#include "shm.h"
#include <chrono>
#include <unistd.h>
#include <cerrno>
#include <sys/fcntl.h>
#include <sys/mman.h>

int is_shm_supported() {
    int shm_fd = shm_open("/test_shm_support", O_CREAT | O_RDWR | O_EXCL, 0600);
    if (shm_fd == -1) {
        // handle error
        if (errno == ENOSYS) { // not supported
            return 0;
        }
        perror("shm_open");
        return 0;
    }

    shm_unlink("/test_shm_support");
    return 1;
}

// The typical workflow is:
// shm_open() - Create/open shared memory object
// ftruncate() - Set its size
// mmap() - Map it into your address space
// Use the memory
// munmap() - Unmap when done
// close() - Close the file descriptor
// shm_unlink() - Remove the object (when last process is done)

//constructor
SharedMemory::SharedMemory(const int& image_size) {
    shm_size = image_size;
    // generate unique name using PID and timestamp
    const auto time_now = std::chrono::high_resolution_clock::now();
    const auto time_stamp = time_now.time_since_epoch().count();
    shm_name = std::format("/shm_pdvu_{}_{}",getpid(),time_stamp);

    shm_fd = shm_open(shm_name.c_str(), O_CREAT | O_RDWR | O_EXCL, 0600); // create memory
    if (shm_fd == -1) {
        throw std::runtime_error("Failed to open shared memory: " + shm_name);
    }

    if (ftruncate(shm_fd, image_size) == -1) { // set size
        close(shm_fd);
        unlink(shm_name.c_str());
        throw std::runtime_error("Failed to set shared memory size: " + shm_name);
    }

    // memory map shared memory object
    mapped_ptr = mmap(nullptr, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (mapped_ptr == MAP_FAILED) {
        close(shm_fd);
        unlink(shm_name.c_str());
        throw std::runtime_error("Failed to map shared memory: " + shm_name);
    }
}

SharedMemory::~SharedMemory() {
    if (mapped_ptr != MAP_FAILED) {
        munmap(mapped_ptr, shm_size);
    }
    close_mem();
    if (!shm_name.empty()) {
        shm_unlink(shm_name.c_str());
        shm_name.clear();
    }
}

const std::string& SharedMemory::name() const{
    return shm_name;
}

const size_t& SharedMemory::size() const {
    return shm_size;
}

void* SharedMemory::data() const {
    return mapped_ptr;
}

void SharedMemory::write_data(const unsigned char* data, const size_t& len) {
    memcpy(mapped_ptr, data, len);
}

void SharedMemory::close_mem() {
    if (shm_fd != -1) {
        close(shm_fd);
        shm_fd = -1;
    }
}

// move constructor
SharedMemory::SharedMemory(SharedMemory &&other) noexcept {
    shm_fd = other.shm_fd;
    shm_name = std::move(other.shm_name);
    mapped_ptr = other.mapped_ptr;
    shm_size = other.shm_size;

    other.shm_name.clear();
    other.shm_fd = -1;
    other.mapped_ptr = MAP_FAILED;
    other.shm_size = 0;
}

SharedMemory& SharedMemory::operator=(SharedMemory &&other) noexcept {
    if (this != &other) {
        close_mem();
        if (mapped_ptr != MAP_FAILED) {
            munmap(mapped_ptr, shm_size);
        }
        if (!shm_name.empty()) {
            shm_unlink(shm_name.c_str());
            shm_name.clear();
        }
        shm_fd = other.shm_fd;
        shm_name = std::move(other.shm_name);
        mapped_ptr = other.mapped_ptr;
        shm_size = other.shm_size;

        other.shm_name.clear();
        other.shm_fd = -1;
        other.mapped_ptr = MAP_FAILED;
        other.shm_size = 0;
    }
    return *this;
}

// TO DO
// logic for rewriting to existing shm instead of allocating a new one
