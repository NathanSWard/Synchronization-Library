// platform.hpp
#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #define SYNC_WINDOWS 1
    #define SYNC_MAC 0
    #define SYNC_LINUX 0
#elif defined(__APPLE__) && TARGET_OS_MAC
    #define SYNC_WINDOWS 0
    #define SYNC_MAC 1
    #define SYNC_LINUX 0
#elif __linux__
    #define SYNC_WINDOWS 0
    #define SYNC_MAC 0
    #define SYNC_LINUX 1
#else
    #error "Unknown compiler"
#endif
