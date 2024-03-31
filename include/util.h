#pragma once

#include <cstdio>

namespace engine {

inline bool VersionFromCharArray(const char* version, int* major, int* minor, int* patch)
{
    if (sscanf(version, "%d.%d.%d", major, minor, patch) != 3) {
        *major = 0;
        *minor = 0;
        *patch = 0;
        return false;
    }
    else {
        return true;
    }
}

} // namespace engine