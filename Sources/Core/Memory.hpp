#pragma once

#include <Core/Logger.hpp>
#include <Core/Types.hpp>

#include <stdlib.h>
#include <string.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug Fill Patterns

#define MEMORY_PATTERN_ALLOC 0xCD
#define MEMORY_PATTERN_FREE 0xDD
#define MEMORY_PATTERN_GUARD 0xFD

#if !defined(MEMORY_GUARD_SIZE)
#define MEMORY_GUARD_SIZE 16
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Memory Stats

struct MemoryStats {
  u64 ActiveAllocations = 0;
  u64 ActiveBytes = 0;
  u64 TotalAllocations = 0;
  u64 TotalFreed = 0;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Memory

class Memory {
public:
  STATIC void *alloc(u64 size) {
#if defined(PIPELINE_DEBUG)
    u64 totalSize = size + (MEMORY_GUARD_SIZE * 2);
    u8 *raw = static_cast<u8 *>(malloc(totalSize));

    if (UNLIKELY(raw == nullptr)) {
      LOG_FATAL("Memory::Alloc failed for %llu bytes.", size);
      return nullptr;
    }

    memset(raw, MEMORY_PATTERN_GUARD, MEMORY_GUARD_SIZE);
    memset(raw + MEMORY_GUARD_SIZE, MEMORY_PATTERN_ALLOC, size);
    memset(raw + MEMORY_GUARD_SIZE + size, MEMORY_PATTERN_GUARD,
           MEMORY_GUARD_SIZE);

    void *userPtr = raw + MEMORY_GUARD_SIZE;

    MemoryStats &stats = getStats();
    stats.ActiveAllocations += 1;
    stats.ActiveBytes += size;
    stats.TotalAllocations += 1;

    return userPtr;
#else
    return malloc(size);
#endif
  }

  STATIC void free(void *ptr, u64 size) {
    if (UNLIKELY(ptr == nullptr)) {
      return;
    }

#if defined(PIPELINE_DEBUG)
    u8 *raw = static_cast<u8 *>(ptr) - MEMORY_GUARD_SIZE;

    checkGuard(raw, "front", ptr);
    checkGuard(raw + MEMORY_GUARD_SIZE + size, "back", ptr);

    memset(raw, MEMORY_PATTERN_FREE, size + (MEMORY_GUARD_SIZE * 2));

    MemoryStats &stats = getStats();
    stats.ActiveAllocations -= 1;
    stats.ActiveBytes -= size;
    stats.TotalFreed += 1;

    ::free(raw);
#else
    (void)size;
    ::free(ptr);
#endif
  }

  STATIC void copy(void *RESTRICT dst, const void *RESTRICT src, u64 size) {
    if (UNLIKELY(size == 0)) {
      return;
    }

    if (UNLIKELY(dst == nullptr || src == nullptr)) {
      LOG_FATAL("Memory::Copy called with a null pointer (dst=%p, src=%p, "
                "size=%llu).",
                dst, src, size);
      return;
    }

    memcpy(dst, src, size);
  }

  STATIC void move(void *dst, const void *src, u64 size) {
    if (UNLIKELY(size == 0)) {
      return;
    }

    if (UNLIKELY(dst == nullptr || src == nullptr)) {
      LOG_FATAL("Memory::Move called with a null pointer (dst=%p, src=%p, "
                "size=%llu).",
                dst, src, size);
      return;
    }

    memmove(dst, src, size);
  }

  STATIC void set(void *dst, u8 value, u64 size) {
    if (UNLIKELY(size == 0)) {
      return;
    }

    if (UNLIKELY(dst == nullptr)) {
      LOG_FATAL("Memory::Set called with a null pointer (size=%llu).", size);
      return;
    }

    memset(dst, value, size);
  }

  STATIC bool compare(const void *a, const void *b, u64 size) {
    if (UNLIKELY(size == 0)) {
      return true;
    }

    if (UNLIKELY(a == nullptr || b == nullptr)) {
      return a == b;
    }

    return memcmp(a, b, size) == 0;
  }

  STATIC MemoryStats &getStats() {
    static MemoryStats s_Stats;
    return s_Stats;
  }

  STATIC void reportLeaks() {
#if defined(PIPELINE_DEBUG)
    const MemoryStats &stats = getStats();

    if (stats.ActiveAllocations > 0) {
      LOG_ERROR("Memory leak detected: %llu active allocation(s), %llu byte(s) "
                "not freed.",
                stats.ActiveAllocations, stats.ActiveBytes);
    } else {
      LOG_INFO("No memory leaks detected.");
    }
#endif
  }

private:
#if defined(PIPELINE_DEBUG)
  STATIC void checkGuard(const u8 *guard, const char *side, void *userPtr) {
    for (u64 i = 0; i < MEMORY_GUARD_SIZE; ++i) {
      if (guard[i] != MEMORY_PATTERN_GUARD) {
        LOG_FATAL("Buffer overrun detected on %s guard of allocation %p.", side,
                  userPtr);
        return;
      }
    }
  }
#endif
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
