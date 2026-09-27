#pragma once

#include <Core/Memory.hpp>
#include <Core/Types.hpp>

#include <string.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// StringView

struct StringView {
  const char *_data;
  u64 _length;

  INLINE StringView() : _data(""), _length(0) {}

  INLINE StringView(const char *str)
      : _data(str), _length(str ? strlen(str) : 0) {}

  INLINE StringView(const char *str, u64 length)
      : _data(str), _length(length) {}

  INLINE const char *cStr() const { return _data; }
  INLINE u64 size() const { return _length; }
  INLINE bool isEmpty() const { return _length == 0; }

  INLINE char operator[](u64 index) const { return _data[index]; }

  INLINE bool operator==(const StringView &other) const {
    if (_length != other._length) {
      return false;
    }

    return memcmp(_data, other._data, _length) == 0;
  }

  INLINE bool operator!=(const StringView &other) const {
    return !(*this == other);
  }

  INLINE u64 find(char c) const {
    for (u64 i = 0; i < _length; ++i) {
      if (_data[i] == c) {
        return i;
      }
    }

    return U64_MAX;
  }

  INLINE StringView subStr(u64 offset, u64 count = U64_MAX) const {
    if (offset >= _length) {
      return StringView(_data + _length, 0);
    }

    u64 maxCount = _length - offset;
    u64 realCount = (count < maxCount) ? count : maxCount;

    return StringView(_data + offset, realCount);
  }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// String

class String {
public:
  INLINE String() : _data(nullptr), _length(0), _capacity(0) {}

  INLINE String(const char *str) {
    _length = str ? strlen(str) : 0;
    _capacity = _length + 1;
    _data = static_cast<char *>(Memory::alloc(_capacity));

    if (_length > 0) {
      memcpy(_data, str, _length);
    }
    _data[_length] = '\0';
  }

  INLINE String(const StringView &view) {
    _length = view._length;
    _capacity = _length + 1;
    _data = static_cast<char *>(Memory::alloc(_capacity));

    if (_length > 0) {
      memcpy(_data, view._data, _length);
    }
    _data[_length] = '\0';
  }

  INLINE String(const String &other) {
    _length = other._length;
    _capacity = _length + 1;
    _data = static_cast<char *>(Memory::alloc(_capacity));

    if (_length > 0) {
      memcpy(_data, other._data, _length);
    }
    _data[_length] = '\0';
  }

  INLINE String(String &&other) noexcept
      : _data(other._data), _length(other._length), _capacity(other._capacity) {
    other._data = nullptr;
    other._length = 0;
    other._capacity = 0;
  }

  INLINE ~String() { Memory::free(_data, _capacity); }

  INLINE String &operator=(const String &other) {
    if (this == &other) {
      return *this;
    }

    Memory::free(_data, _capacity);

    _length = other._length;
    _capacity = _length + 1;
    _data = static_cast<char *>(Memory::alloc(_capacity));

    if (_length > 0) {
      memcpy(_data, other._data, _length);
    }
    _data[_length] = '\0';

    return *this;
  }

  INLINE String &operator=(String &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    Memory::free(_data, _capacity);

    _data = other._data;
    _length = other._length;
    _capacity = other._capacity;

    other._data = nullptr;
    other._length = 0;
    other._capacity = 0;

    return *this;
  }

  INLINE const char *cStr() const { return _data ? _data : ""; }
  INLINE u64 size() const { return _length; }
  INLINE bool isEmpty() const { return _length == 0; }

  INLINE char operator[](u64 index) const { return _data[index]; }

  INLINE char &operator[](u64 index) { return _data[index]; }

  INLINE bool operator==(const String &other) const {
    if (_length != other._length) {
      return false;
    }

    return memcmp(_data, other._data, _length) == 0;
  }

  INLINE bool operator!=(const String &other) const {
    return !(*this == other);
  }

  INLINE String operator+(const StringView &other) const {
    String result;

    result._length = _length + other._length;
    result._capacity = result._length + 1;
    result._data = static_cast<char *>(Memory::alloc(result._capacity));

    if (_length > 0) {
      memcpy(result._data, _data, _length);
    }
    if (other._length > 0) {
      memcpy(result._data + _length, other._data, other._length);
    }
    result._data[result._length] = '\0';

    return result;
  }

  INLINE operator StringView() const { return StringView(cStr(), _length); }

private:
  char *_data;
  u64 _length;
  u64 _capacity;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////