#ifdef __cplusplus
extern "C"
{
#endif
#ifndef FALSE
#define FALSE (0)
#define TRUE  (1)
#endif
#ifndef NULL
#define NULL  (void*)(0)
#endif
#ifndef offsetof
#define offsetof(type, member) ( (int) & ((type*)0) -> member )
#endif
typedef unsigned long long  u64;
typedef long long           s64;
typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;
typedef signed char    s8;
typedef signed short   s16;
typedef int            s32;
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

extern int debug;
extern void exit_error(char *format,...);
extern void *myalloc(char *str,size_t size);
extern void debug_printf(char *format,...);
void myread(int fd,void *buf, size_t count);
void mywrite(int fd,void *buf, size_t count);
#ifdef __cplusplus
}
#endif
