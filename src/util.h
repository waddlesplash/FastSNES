#pragma once

#ifdef SNEM_LOG
void snemlog(const char* format, ...);
#else
#  define snemlog(...)
#endif
