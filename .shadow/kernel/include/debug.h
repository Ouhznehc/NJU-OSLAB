#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef assert
#undef assert
#endif
#ifdef panic
#undef panic
#endif
#ifdef panic_on
#undef panic_on
#endif

#define __DEBUG

#ifdef __DEBUG

    #define Log(format, ...) \
    printf("\33[1;35m[%s,%d,%s] " format "\33[0m\n", \
        __FILE__, __LINE__, __func__, ## __VA_ARGS__);

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
    #define Log(format,...)
    #define panic(format, ...)
    #define panic_on(cond,format,...) 
    #define assert(cond)
    #define Assert(cond,format,...)
#endif



#endif