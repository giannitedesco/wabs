#ifndef _WA_PROTOCOK_H
#define _WA_INTERNAL_H

struct _wa {
	struct sockaddr_in sa;
	char nick[17];
	uint8_t playerid;
	int s;
	int go;
	wgt_t wgt;
	uint8_t *msg_head;
	uint8_t *msg_tail;
	uint8_t buf[8192];
};

#endif /* _WA_INTERNAL_H */
