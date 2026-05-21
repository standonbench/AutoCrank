#ifndef DEBUG_H
#define DEBUG_H

#ifndef DEBUG_BUILD
#define DEBUG_BUILD 0
#endif

extern PlaydateAPI* pd;

#if DEBUG_BUILD
  #define DLOG(fmt, ...) pd->system->logToConsole(fmt, ##__VA_ARGS__)
  #define ASSERT(cond) do { \
      if (!(cond)) { \
          pd->system->error("ASSERT FAIL [%s:%d]: %s", __FILE__, __LINE__, #cond); \
      } \
  } while(0)
#else
  #define DLOG(fmt, ...) ((void)0)
  #define ASSERT(cond) ((void)0)
#endif

#endif // DEBUG_H
