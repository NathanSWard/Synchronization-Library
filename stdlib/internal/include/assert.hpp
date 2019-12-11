// assert.hpp
#pragma once

#include "platform.hpp"

#ifdef DISABLE_SYNC_ASSERT

#define SYNC_ASSERT(b, msg) (void)b
#define SYNC_POSIX_ASSERT(fn, msg) (void)fn
#define SYNC_WINDOWS_ASSERT(fn, msg) (void)fn

#else

#include <cassert>
#include <iostream>
#include <string_view>

inline void _sync_assert(bool b, std::string_view msg) {
    if (!b) {
        std::cerr << "SYNC_ASSERT: " << msg << "\n";
        assert(false);
    }
}

#define SYNC_ASSERT(b, msg) _sync_assert(b, msg);

    #if SYNC_MAC || SYNC_LINUX

    #define SYNC_POSIX_ASSERT(fn, msg) _sync_assert(fn == 0, msg)

    #elif SYNC_WINDOWS

    #define SYNC_WINDOWS_ASSERT(fn, msg) _sync_assert(fn != 0, msg)

    #endif

#endif