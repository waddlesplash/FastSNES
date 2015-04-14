#include "util.h"

#include <stdio.h>

#ifdef SNEM_LOG
FILE* snemlogf;
void snemlog(const char* format, ...)
{
	char buf[256];
	return;
	if (!snemlogf)
		snemlogf = fopen("snemlog.txt", "wt");
	// return;
	va_list ap;
	va_start(ap, format);
	vsprintf(buf, format, ap);
	va_end(ap);
	fputs(buf, snemlogf);
	fflush(snemlogf);
}
#endif
