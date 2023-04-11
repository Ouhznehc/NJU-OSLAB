#ifndef __COMMON_H__
#define __COMMON_H__

#include <kernel.h>
#include <klib.h>
#include <klib-macros.h>
#include "spinlock.h"
#include "pmm.h"

#define __DEBUG_MODE__

#ifdef assert
#undef assert
#endif
#ifdef panic
#undef panic
#endif
#ifdef panic_on
#undef panic_on
#endif

#ifdef __DEBUG_MODE__


    extern spinlock_t debug_lk;

    #define debug(format, ...) \
        printf("\33[1;35m[%s: %d] " format "\33[0m\n", \
            __func__, __LINE__, ## __VA_ARGS__);

    #define kernal_panic(format, ...) \
        do { \
            debug("\33[1;31msystem panic: " format, ## __VA_ARGS__); \
            halt(1); \
        } while (0)

    #define Log(format, ...) \
        do{ \
        spin_lock(&debug_lk); \
        printf("\33[1;35m[%s: %03d] " format "\33[0m\n", \
            __func__, __LINE__, ## __VA_ARGS__); \
        spin_unlock(&debug_lk); \
        }while(0)

    #define panic(format, ...) \
        do { \
            Log("\33[1;31msystem panic: " format, ## __VA_ARGS__); \
            halt(1); \
        } while (0)

    #define panic_on(cond,format,...) \
        do{ \
            if(cond) { \
            panic(format); \
            }  \
        }while(0)

    #define assert(cond) \
        do { \
            if (!(cond)) { \
            panic("Assertion failed: %s", #cond); \
            } \
        } while (0)

    #define Assert(cond,format,...) \
        do{ \
            if(!(cond)) { \
            panic(format); \
            }  \
        }while(0)

    #define TODO() panic("please implement me")


#else
    #define kernal_panic(format, ...)
    #define Log(format,...)           
    #define panic(format, ...)        
    #define panic_on(cond,format,...) 
    #define assert(cond)              
    #define Assert(cond,format,...)   
#endif

#endif