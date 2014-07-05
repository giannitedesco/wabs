#ifndef _WA_PROTOCOK_H
#define _WA_INTERNAL_H

#include <sys/socket.h>
#include <netinet/in.h>
struct _wa {
	struct sockaddr_in sa;
	char nick[17];
	uint8_t playerid;
	uint8_t frame;
	int s;
	int go;
	wgt_t wgt;
	uint8_t *msg_head;
	uint8_t *msg_tail;
	uint8_t buf[8192];
};

int wa__wait(struct _wa *wa);
int wa__send(struct _wa *wa, const void *buf, size_t len);
int wa__recv(struct _wa *wa);
const struct wa_hdr *wa__next_msg(struct _wa *wa);

int wa__dispatch_lobby(wa_t wa, const struct wa_hdr *r);
int wa__dispatch_game(wa_t wa, const struct wa_frame *f);

#endif /* _WA_INTERNAL_H */
