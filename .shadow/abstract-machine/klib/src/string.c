#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

void *memcpy(void *out, const void *in, size_t n);


size_t strlen(const char *s) {
  size_t len = 0;
  while (*s++ != '\0') len++;
  return len;
}

char *strcpy(char *dst, const char *src) {
  if(dst == NULL || src == NULL) return NULL;
  char *ret = dst;
  while((*dst++ = *src++) != '\0');
  return ret;
}

char *strncpy(char *dst, const char *src, size_t n) {
  if (dst == NULL || src == NULL) return NULL;
  char *ret = dst;
  size_t offset = 0, len = strlen(src);
  if (n > len) {
    offset = n - len;
    n = len;
  }
  while (n--) *dst++ = *src++;
  while (offset--) *dst++ = '\0';
  return ret;

}

char *strcat(char *dst, const char *src) {
  if(dst == NULL || src == NULL) return NULL;
  char *start = dst + strlen(dst);
  while((*start++ = *src++) != '\0');
  return dst;
}

int strcmp(const char *s1, const char *s2) {
  assert(s1 != NULL && s2 != NULL);
  while((*s1) && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  int ret = *(unsigned char*)s1 - *(unsigned char*)s2;
  if (ret < 0) return -1;
  else if (ret > 0) return 1;
  else return 0;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  assert(s1 != NULL && s2 != NULL);    
  unsigned char *str1 = (unsigned char*) s1;
  unsigned char *str2 = (unsigned char*) s2;   
  while (n--) { 
    if (*str1 == *str2) { 
      str1++;                     
      str2++; 
    } 
    else return *str1 < *str2 ? -1 : 1;
  }       
  return 0; 
}

void *memset(void *s, int c, size_t n) {
  if (s == NULL) return NULL;
  unsigned char *str = (unsigned char*) s;
  while (n--) *str++ = c;
  return s;
}

void *memmove(void *dst, const void *src, size_t n) {
  return memcpy(dst, src, n);
}

void *memcpy(void *out, const void *in, size_t n) {
  if (out == NULL || in == NULL) return NULL;
  if(n > 16777216) panic("memcpy copy data larger than 16MB!");
  unsigned char *dst = (unsigned char*) out;
  unsigned char *src = (unsigned char*) in;
  if (dst >= src && dst <= src + n - 1) {
        dst = dst + n - 1;
        src = src + n - 1;
        while (n--)
            *dst-- = *src--;
    }
    else {
        while (n--)
            *dst++ = *src++;
    }
    return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  assert(s1 != NULL && s2 != NULL); 
  unsigned char *str1 = (unsigned char*) s1;
  unsigned char *str2 = (unsigned char*) s2;      
  while (n--) { 
    if (*str1 == *str2) { 
      str1++;                     
      str2++; 
    } 
    else return *str1 < *str2 ? -1 : 1;
  }       
  return 0; 
}
#endif
