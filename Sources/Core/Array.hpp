#pragma once

#include <Core/Memory.hpp>
#include <Core/Types.hpp>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Array

template <typename T> class Array {
public:
  INLINE Array() : _data(nullptr), _count(0), _capacity(0) {}

  INLINE Array(Array &&other) noexcept
      : _data(other._data), _count(other._count), _capacity(other._capacity) {
    other._data = nullptr;
    other._count = 0;
    other._capacity = 0;
  }

  INLINE Array &operator=(Array &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    destroyAll();
    Memory::free(_data, _capacity * sizeof(T));

    _data = other._data;
    _count = other._count;
    _capacity = other._capacity;

    other._data = nullptr;
    other._count = 0;
    other._capacity = 0;

    return *this;
  }

  Array(const Array &) = delete;
  Array &operator=(const Array &) = delete;

  INLINE ~Array() {
    destroyAll();
    Memory::free(_data, _capacity * sizeof(T));
  }

  INLINE void add(const T &value) {
    ensureCapacity(_count + 1);
    construct(&_data[_count], value);
    _count += 1;
  }

  INLINE void add(T &&value) {
    ensureCapacity(_count + 1);
    construct(&_data[_count], static_cast<T &&>(value));
    _count += 1;
  }

  INLINE u64 size() const { return _count; }
  INLINE bool isEmpty() const { return _count == 0; }

  INLINE T &operator[](u64 index) { return _data[index]; }
  INLINE const T &operator[](u64 index) const { return _data[index]; }

  INLINE T *begin() { return _data; }
  INLINE T *end() { return _data + _count; }
  INLINE const T *begin() const { return _data; }
  INLINE const T *end() const { return _data + _count; }

private:
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void *operator new(size_t, void *ptr) noexcept;

  template <typename... Args> STATIC void construct(T *ptr, Args &&...args) {
    ::new (static_cast<void *>(ptr)) T(static_cast<Args &&>(args)...);
  }

  STATIC void destroy(T *ptr) { ptr->~T(); }

  INLINE void ensureCapacity(u64 required) {
    if (required <= _capacity) {
      return;
    }

    u64 newCapacity = (_capacity == 0) ? 8 : (_capacity * 2);
    if (newCapacity < required) {
      newCapacity = required;
    }

    T *newData = static_cast<T *>(Memory::alloc(newCapacity * sizeof(T)));

    for (u64 i = 0; i < _count; ++i) {
      construct(&newData[i], static_cast<T &&>(_data[i]));
      destroy(&_data[i]);
    }

    if (_data != nullptr) {
      Memory::free(_data, _capacity * sizeof(T));
    }

    _data = newData;
    _capacity = newCapacity;
  }

  INLINE void destroyAll() {
    for (u64 i = 0; i < _count; ++i) {
      destroy(&_data[i]);
    }

    _count = 0;
  }

  T *_data;
  u64 _count;
  u64 _capacity;
};

inline void *operator new(size_t, void *ptr) noexcept { return ptr; }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////