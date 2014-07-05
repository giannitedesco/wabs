#define _GNU_SOURCE
#include <sys/types.h>
#include <netinet/tcp.h>
#include <unistd.h>

#include <worms/worms.h>
#include <worms/wa.h>
#include <worms/wgt.h>
#include <worms/wa-protocol.h>

#include "wa-internal.h"

static int wa__login(wa_t wa, const char *nick)
{
	const struct wa_hdr *r;
	struct wa_login l;
	struct wa_login2 l2;

	memset(&l, 0, sizeof(l));
	l.hdr.chan = 1;
	l.hdr.len = sizeof(l);
	l.hdr.command = WORMS_COMMAND_LOGIN;

	snprintf(l.name, sizeof(l.name), "%s", nick);
	snprintf(l.gamename, sizeof(l.gamename), "cunt");
	l.vers[0] = 0xa7;
	l.vers[1] = 0x24;
	l.vers[2] = 0xf4;

	if ( !wa__send(wa, &l, sizeof(l)) )
		return 0;

	r = wa__next_msg(wa);
	if ( NULL == r )
		return 0;

	if ( r->command != WORMS_SERVER_LOGIN_OK ) {
		printf("bad login\n");
		return 0;
	}else{
		printf("login OK\n");
	}

	memset(&l2, 0, sizeof(l2));
	l2.hdr.chan = 1;
	l2.hdr.len = sizeof(l2);
	l2.hdr.command = WORMS_COMMAND_LOGIN2;

	snprintf(l2.name, sizeof(l2.name), "%s", nick);
	l2.country = 0xf;

	if ( !wa__send(wa, &l2, sizeof(l2)) )
		return 0;

	snprintf(wa->nick, sizeof(wa->nick), "%s", nick);
	return 1;
}

int wa_mainloop(wa_t wa)
{
	const struct wa_hdr *r;
	for(;wa->go;) {
		r = wa__next_msg(wa);
		if ( NULL == r )
			return 0;
		switch(r->chan) {
		case WORMS_CHAN_LOBBY:
			wa__dispatch_lobby(wa, r);
			break;
		case WORMS_CHAN_GAME:
			wa__dispatch_game(wa, r);
			break;
		default:
			printf("Unknown chan: 0x%.2x msg=0x%.2x len=%u\n",
				r->chan, r->command, r->len);
			hex_dump(r->data, r->len - sizeof(*r), 0);
			break;
		}
	}

	return 1;
}

void wa_exit_mainloop(wa_t wa)
{
	wa->go = 0;
}

int wa_chat(wa_t wa, const char *type, const char *to, const char *msg)
{
	struct {
		struct wa_hdr hdr;
		char msg[strlen(wa->nick) +
			strlen(type) +
			((to) ? strlen(to) : strlen("ALL")) +
			strlen(msg) + 16];
	}buf;
	int len;

	len = snprintf(buf.msg, sizeof(buf.msg), "%s:%s:%s:%s",
			type, wa->nick,
			(to) ? to : "ALL", msg);
	memset(&buf.hdr, 0, sizeof(buf.hdr));
	buf.hdr.chan = 1;
	buf.hdr.unknown = 0;
	buf.hdr.len = len + sizeof(buf.hdr) + 1;
	buf.hdr.command = WORMS_SERVER_CHAT;
	printf(">>> %.*s\n", (int)sizeof(buf.msg), buf.msg);
	return wa__send(wa, &buf, buf.hdr.len);
}

int wa_ready(wa_t wa, int ready)
{
	struct wa_ready rdy;

	memset(&rdy, 0, sizeof(rdy));
	rdy.hdr.chan = 1;
	rdy.hdr.len = sizeof(rdy);
	rdy.hdr.command = WORMS_SERVER_READY;
	rdy.playerid = wa->playerid;
	rdy.ready = !!ready;

	return wa__send(wa, &rdy, sizeof(rdy));
}

const char *wa_get_nick(wa_t wa)
{
	return wa->nick;
}

wa_t wa_connect(uint32_t ip, uint16_t port, const char *nick)
{
	struct _wa *wa;

	wa = calloc(1, sizeof(*wa));
	if ( NULL == wa ) {
		fprintf(stderr, "%s: calloc: %s\n", cmd, sys_err());
		goto out;
	}

	wa->s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if ( wa->s < 0 ) {
		fprintf(stderr, "%s: socket: %s\n", cmd, sys_err());
		goto out_free;
	}

	wa->sa.sin_family = AF_INET;
	wa->sa.sin_addr.s_addr = ip;
	wa->sa.sin_port = htons(port);
	if ( connect(wa->s, (struct sockaddr *)&wa->sa, sizeof(wa->sa)) ) {
		fprintf(stderr, "%s: connect: %s\n", cmd, sys_err());
		goto out_close;
	}

	wa->msg_head = wa->buf;
	wa->msg_tail = wa->buf;
	wa->go = 1;

	wa->wgt = wgt_open("/home/scara/wormsarm/User/Teams/WG.WGT");

	printf("connected\n");
	if ( !wa__login(wa, nick) )
		goto out_close;

	goto out;

out_close:
	close(wa->s);
out_free:
	free(wa);
	wa = NULL;
out:
	return wa;
}

void wa_close(wa_t wa)
{
	if ( wa ) {
		wgt_close(wa->wgt);
		close(wa->s);
		free(wa);
	}
}
