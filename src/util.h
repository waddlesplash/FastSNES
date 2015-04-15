#pragma once
#include <stdio.h>

#ifdef SNEM_LOG
void snemlog(const char* format, ...);
#  define snemdebug(...) printf(__VA_ARGS__);
#else
#  define snemlog(...)
#  define snemdebug(format, ...)
#endif
