#ifndef _WA_PROTOCOK_H
#define _WGT_FORMAT_H

typedef struct _wgt *wgt_t;

wgt_t wgt_open(const char *fn);
void wgt_close(wgt_t w);

#endif /* _WGT_FORMAT_H */
