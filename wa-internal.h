#ifndef _WA_PROTOCOK_H
#define _WA_INTERNAL_H

struct _wa {
	struct sockaddr_in sa;
	char *nick;
	int s;
	int go;
	uint8_t *msg_head;
	uint8_t *msg_tail;
	uint8_t buf[8192];
};

#endif /* _WA_INTERNAL_H */
