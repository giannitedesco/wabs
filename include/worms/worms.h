#ifndef _WA_PROTOCOK_H
#define _WORMS_H

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

void hex_dumpf(FILE *f, const void *buf, size_t len, size_t llen);
void hex_dump(const void *ptr, size_t len, size_t llen);

extern const char *cmd;
static inline const char *sys_err(void)
{
	return strerror(errno);
}

#endif /* _WORMS_H */
