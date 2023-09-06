#pragma once

#include <cstring>
#include <cstdio>
#include <unistd.h>

// Convert the pointer type to a string 
// in the form 0xYYYYYY and write it to buf.
// The return value is the pointer value after writing.
// ex) when write 0x1234 then return value is buf+6.
[[maybe_unused]]
char *format_built_in_type(char *buf, void *ptr) noexcept;

// Convert the size_t type to a decimal string 
// and write it to buf.
// The return value is the pointer value after writing.
[[maybe_unused]]
char *format_built_in_type(char *buf, size_t n) noexcept;

// This function writes s to buf.
// The return value is the pointer value after writing.
[[maybe_unused]] 
char *format_built_in_type(char *buf, const char *s) noexcept;

[[maybe_unused]]
void format(char *buf) noexcept;

// Convert multiple arguments to strings, pack them into buf, and write them.
// The only values that can be passed as arguments are instances of 
// the following types now.
// void *, size_t , const char *
// 
// For example, format(buf, "hello world", 1, ", ", ptr);
// would write buf = "hello world1, 0xxxxxxxx".
template <typename Head, typename... Tail>
[[maybe_unused]]
void format(char *buf, Head head, Tail... tail) noexcept {
  buf = format_built_in_type(buf, head);
  format(buf, tail...);
}

template <typename Head>
[[maybe_unused]]
void format_as_csv_entry(char *buf, Head head) noexcept {
  buf = format_built_in_type(buf, head);
  buf = format_built_in_type(buf, "\n");
  *buf = '\0';
}

template <typename Head, typename... Tail>
[[maybe_unused]]
void format_as_csv_entry(char *buf, Head head, Tail... tail) noexcept {
  buf = format_built_in_type(buf, head);
  buf = format_built_in_type(buf, ", ");
  format_as_csv_entry(buf, tail...);
}

template <typename... Args, size_t BufSize = 0x400>
[[maybe_unused]]
void write_to_stderr(Args... args) {
  char buf[BufSize];
  format(buf, args...);
  write(STDERR_FILENO, buf, strlen(buf));
}

[[maybe_unused]]
inline bool is_power_of_2(size_t x) noexcept {
  return x > 0 && ((x - 1) & x) == 0;
}

[[maybe_unused]]
inline bool is_valid_alignment(size_t alignment) noexcept {
    return is_power_of_2(alignment) && (alignment % sizeof(void *) == 0);
}

// if x > 0x80000000'00000000 then return 0
[[maybe_unused]]
inline size_t next_power_of_2(size_t x) noexcept {
  constexpr size_t max_x = 1ull << 63;
  if (x > max_x)
    return 0;
  size_t retval = 1;
  while (retval < x) 
    retval <<= 1;
  return retval;
}