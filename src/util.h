#pragma once
#include <stdio.h>

#ifdef SNEM_LOG
#  define snemlog(...) printf(__VA_ARGS__);
#  define snemdebug(...) printf(__VA_ARGS__);
#else
#  define snemlog(...)
#  define snemdebug(format, ...)
#endif
