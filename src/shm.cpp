#include "shm.h"
#include <chrono>
#include <unistd.h>
#include <cerrno>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <atomic>
static std::atomic<int> shm_sequence_id{0};

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

//constructor
SharedMemory::SharedMemory(size_t image_size) {
    shm_size = image_size;
    // generate unique name using PID and timestamp
    int id = shm_sequence_id.fetch_add(1);
    shm_name = std::format("/pdvu_{}_{}",getpid(),id);

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
