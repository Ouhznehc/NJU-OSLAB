#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

static char my_str[4096];

char* int_to_string(int num, char* ans, int zeroflag, int field_width) {
  int sign = (num >= 0);
  int counter = 0;
  if (!sign) num = -num;
  char reverse[1024];
  char* s = reverse;
  if (num == 0) { *s++ = '0'; counter++; }
  else while (num) {
    *s++ = num % 10 + '0';
    counter++;
    num /= 10;
  }
  *s = '\0';
  size_t len = strlen(reverse);
  if (!sign) *ans++ = '-';
  if (field_width != -1) {
    for (size_t i = 0; i < field_width - counter; i++)
      *ans++ = zeroflag ? '0' : ' ';
  }
  for (size_t i = 0; i < len; i++) *ans++ = *(--s);
  return ans;
}

char* uint_to_string(uint32_t num, char* ans, int zeroflag, int field_width) {
  int counter = 0;
  char reverse[1024];
  char* s = reverse;
  if (num == 0) { *s++ = '0'; counter++; }
  else while (num) {
    if (num % 16 > 9) *s++ = num % 16 - 10 + 'a';
    else *s++ = num % 16 + '0';
    counter++;
    num /= 16;
  }
  *s = '\0';
  size_t len = strlen(reverse);
  *ans++ = '0'; *ans++ = 'x';
  if (field_width != -1) {
    for (size_t i = 0; i < field_width - counter; i++)
      *ans++ = zeroflag ? '0' : ' ';
  }
  for (size_t i = 0; i < len; i++) *ans++ = *(--s);
  return ans;
}

int vsprintf(char* out, const char* fmt, va_list ap) {
  char* str;
  int num;
  char* s;
  int zeroflag = false, field_width = -1;
  for (str = out; *fmt; fmt++) {
    if (*fmt != '%') { *str++ = *fmt; continue; }
    fmt++;
    zeroflag = false, field_width = -1;
    if (*fmt == '0') { fmt++; zeroflag = true; }
    if (*fmt >= '0' && *fmt <= '9') {
      field_width = atoi(fmt);
      while (*fmt >= '0' && *fmt <= '9') fmt++;
    }
    switch (*fmt) {
    case 'd':
      num = va_arg(ap, int);
      str = int_to_string(num, str, zeroflag, field_width);
      continue;
    case 'p':
      num = va_arg(ap, uint32_t);
      str = uint_to_string(num, str, zeroflag, field_width);
      continue;
    case 's':
    case 'c':
      s = va_arg(ap, char*);
      size_t len = strlen(s);
      if (field_width != -1) {
        for (size_t i = 0; i < field_width - len; i++)
          *str++ = zeroflag ? '0' : ' ';
      }
      for (size_t i = 0; i < len; i++) *str++ = *s++;
      continue;
    default: break;
    }
  }
  *str = '\0';
  return str - out;
}

int sprintf(char* out, const char* fmt, ...) {
  va_list args;
  int val;
  va_start(args, fmt);
  val = vsprintf(out, fmt, args);
  va_end(args);
  return val;
}

int snprintf(char* out, size_t n, const char* fmt, ...) {
  panic("Not implemented");
}

int vsnprintf(char* out, size_t n, const char* fmt, va_list ap) {
  panic("Not implemented");
}

int printf(const char* fmt, ...) {
  int val;
  va_list args;
  va_start(args, fmt);
  val = vsprintf(my_str, fmt, args);
  va_end(args);
  putstr(my_str);
  return val;
}
#endif
