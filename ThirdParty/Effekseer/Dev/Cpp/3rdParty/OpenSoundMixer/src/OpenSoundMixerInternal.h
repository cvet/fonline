
#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

#define NOMINMAX
#include <algorithm>

#include <chrono>
#include <thread>

#if _WIN32
#define STRICMP _stricmp
#else
#define STRICMP strcasecmp
#endif

#if _WIN32
#include <Windows.h>
#endif

namespace osm {
template <class T>
void SafeAddRef(T& t) {
    if (t != NULL) {
        t->AddRef();
    }
}

template <class T>
void SafeRelease(T& t) {
    if (t != NULL) {
        t->Release();
        t = NULL;
    }
}

template <class T>
void SafeSubstitute(T& target, T& value) {
    SafeAddRef(value);
    SafeRelease(target);
    target = value;
}

template <typename T>
inline void SafeDelete(T*& p) {
    if (p != NULL) {
        delete (p);
        (p) = NULL;
    }
}

template <typename T>
inline void SafeDeleteArray(T*& p) {
    if (p != NULL) {
        delete[](p);
        (p) = NULL;
    }
}

/**
@brief	clamp
*/
template <typename T, typename U, typename V>
T Clamp(T t, U max_, V min_) {
    if (t > (T)max_) {
        t = (T)max_;
    }

    if (t < (T)min_) {
        t = (T)min_;
    }

    return t;
}

struct Sample32 {
    int32_t Left;
    int32_t Right;
};

struct Sample4ch32 {
    std::array<int32_t, 4> Values;
};

}  // namespace osm
