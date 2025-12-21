#pragma once

#include <cstddef>

inline std::size_t getCurrentRSS();

#if defined(__APPLE__)

#include <mach/mach.h>

inline std::size_t getCurrentRSS() {
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;

    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS) {
        return 0;
                  }
    return info.resident_size;
}
/* only tested on mac so far */
#elif defined(__linux__)

#include <unistd.h>
#include <fstream>

inline std::size_t getCurrentRSS() {
    std::ifstream statm("/proc/self/statm");
    long pages = 0, rss = 0;
    statm >> pages >> rss;
    return (std::size_t)rss * sysconf(_SC_PAGESIZE);
}

#elif defined(_WIN32)

#include <windows.h>
#include <psapi.h>

inline std::size_t getCurrentRSS() {
    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
    return (std::size_t)info.WorkingSetSize;
}

#else
#error "Unsupported platform"
#endif