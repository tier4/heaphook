#include "heaphook/utils.hpp"

char * format_built_in_type(char * buf, void * ptr) noexcept
{
  buf[0] = '0';
  buf[1] = 'x';
  int bits = 8 * sizeof(void *);
  int shamt = bits - 4;
  int idx = 2;
  while (shamt >= 0) {
    size_t mask = 0b1111ull << shamt;
    size_t n = (reinterpret_cast<size_t>(ptr) & mask) >> shamt;
    if (n < 10) {
      buf[idx] = '0' + n;
    } else {
      buf[idx] = 'a' + n - 10;
    }
    idx++;
    shamt -= 4;
  }
  buf += idx;
  return buf;
}

char * format_built_in_type(char * buf, size_t n) noexcept
{
  if (n == 0) {
    *(buf++) = '0';
    return buf;
  }
  // n != 0
  char tmp[21]; // Maximum value of unsigned long long is 20 digits in decimal
  char * ptr = &tmp[20];
  *(ptr--) = '\0';
  while (n) {
    *ptr = '0' + (n % 10);
    n /= 10;
    ptr--;
  }
  ptr++;
  while (*ptr) {
    *(buf++) = *(ptr++);
  }
  return buf;
}

char * format_built_in_type(char * buf, const char * s) noexcept
{
  while (*s) {
    *(buf++) = *(s++);
  }
  return buf;
}

void format(char * buf) noexcept
{
  *buf = '\0';
}
