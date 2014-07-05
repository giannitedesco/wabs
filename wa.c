#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <readline/readline.h>
#include <readline/history.h>

#include <worms/worms.h>
#include <worms/wa.h>
#include <worms/wgt.h>
#include <worms/wa-protocol.h>

#include "wa-internal.h"

static int wa__wait(struct _wa *wa)
{
	struct pollfd pfd[2];

wait_more:
	pfd[0].fd = STDIN_FILENO;
	pfd[0].events = POLLIN;
	pfd[0].revents = 0;
	pfd[1].fd = wa->s;
	pfd[1].events = POLLIN;
	pfd[0].revents = 0;

	if ( poll(pfd, 2, -1) < 0 ) {
		if ( errno == EINTR )
			goto wait_more;
		fprintf(stderr, "%s: poll: %s\n", cmd, sys_err());
		return 0;
	}
	if ( pfd[0].revents )
		rl_callback_read_char();
	if ( pfd[1].revents || !wa->go )
		return 1;
	else
		goto wait_more;

	return 1;
}

static int wa__send(struct _wa *wa, const void *buf, size_t len)
{
	const uint8_t *ptr = buf, *end = buf + len;
	ssize_t ret;

	//printf("Sending %zu bytes:\n", len);
	//hex_dump(buf, len, 0);

again:
	ret = send(wa->s, ptr, end - ptr, MSG_NOSIGNAL);
	if ( ret < 0 ) {
		fprintf(stderr, "%s: send: %s\n", cmd, sys_err());
		return 0;
	}

	if ( ret == 0 ) {
		fprintf(stderr, "%s: send: connection closed by peer\n", cmd);
		return 0;
	}

	ptr += (size_t)ret;
	if ( ptr < end )
		goto again;

	return 1;
}

static int wa__recv(struct _wa *wa)
{
	ssize_t ret;

wait_more:
	if ( !wa__wait(wa) ) {
		return 0;
	}
	if ( !wa->go )
		return 0;

	ret = recv(wa->s, wa->msg_tail,
			(wa->buf + sizeof(wa->buf)) - wa->msg_tail,
			 MSG_NOSIGNAL|MSG_DONTWAIT);
	if ( ret < 0 ) {
		if ( errno == EAGAIN )
			goto wait_more;
		fprintf(stderr, "%s: send: %s\n", cmd, sys_err());
		return 0;
	}

	if ( ret == 0 ) {
		fprintf(stderr, "%s: recv: connection closed by peer\n", cmd);
		return 0;
	}

	//printf("recv %zd bytes\n", ret);
	wa->msg_tail += ret;
	return 1;
}

static const struct wa_hdr *wa__next_msg(struct _wa *wa)
{
	const struct wa_hdr *hdr;
	const uint8_t *end;

	//printf("\n");
	assert(wa->msg_tail >= wa->msg_head);
	assert(wa->msg_head >= wa->buf);
	assert(wa->msg_tail <= wa->buf + sizeof(wa->buf));
	assert(wa->msg_head <= wa->buf + sizeof(wa->buf));

	if ( wa->msg_tail == wa->msg_head ) {
		//printf("reset buffer\n");
		wa->msg_tail = wa->msg_head = wa->buf;
	}

	while ( (size_t)(wa->msg_tail - wa->msg_head) < sizeof(*hdr) ) {
		//printf("reading for header: %zu < %zu\n",
		//		(size_t)(wa->msg_tail - wa->msg_head),
		//		sizeof(*hdr));
		if ( !wa__recv(wa) )
			return NULL;
	}

	hdr = (struct wa_hdr *)wa->msg_head;
	assert(hdr->h_len < sizeof(wa->buf));
	assert(hdr->h_len >= sizeof(*hdr));
	end = wa->msg_head + hdr->h_len;
	while(wa->msg_tail < end) {
		//printf("reading for body %zu < %zu\n",
		//	wa->msg_tail - wa->msg_head, end - wa->msg_head);
		if ( !wa__recv(wa) )
			return NULL;
	}

	//printf("Dispatching %u bytes\n", hdr->h_len);
	//hex_dump(hdr, hdr->h_len, 0);
	wa->msg_head += hdr->h_len;
	return hdr;
}

static int wa__login(wa_t wa, const char *nick)
{
	const struct wa_hdr *r;
	struct wa_login l;
	struct wa_login2 l2;

	memset(&l, 0, sizeof(l));
	l.l_hdr.h_chan = 1;
	l.l_hdr.h_len = sizeof(l);
	l.l_hdr.h_command = WORMS_COMMAND_LOGIN;

	snprintf(l.l_name, sizeof(l.l_name), "%s", nick);
	snprintf(l.l_gamename, sizeof(l.l_gamename), "cunt");
	l.l_vers[0] = 0xa7;
	l.l_vers[1] = 0x24;
	l.l_vers[2] = 0xf4;

	if ( !wa__send(wa, &l, sizeof(l)) )
		return 0;

	r = wa__next_msg(wa);
	if ( NULL == r )
		return 0;

	if ( r->h_command != WORMS_SERVER_LOGIN_OK ) {
		printf("bad login\n");
		return 0;
	}else{
		printf("login OK\n");
	}

	memset(&l2, 0, sizeof(l2));
	l2.l_hdr.h_chan = 1;
	l2.l_hdr.h_len = sizeof(l2);
	l2.l_hdr.h_command = WORMS_COMMAND_LOGIN2;

	snprintf(l2.l_name, sizeof(l2.l_name), "%s", nick);
	l2.l_country = 0xf;

	if ( !wa__send(wa, &l2, sizeof(l2)) )
		return 0;

	snprintf(wa->nick, sizeof(wa->nick), "%s", nick);
	return 1;
}

static int wa__chat(wa_t wa, const struct wa_hdr *r)
{
	printf("Chat: %.*s\n", (int)(r->h_len - sizeof(*r)), r->data);
	return 1;
}

static int wa__ready(wa_t wa, const struct wa_hdr *r)
{
	const struct wa_ready *rdy = (struct wa_ready *)r;
	printf("Player %u %sready!\n",
		rdy->r_playerid, (rdy->r_ready) ? "" : "not ");
	return 1;
}

static int wa__player_list(wa_t wa, const struct wa_hdr *r)
{
	const struct wa_playerlist *p = (struct wa_playerlist *)r;
	unsigned int i, cnt;

	printf("Player list: %u players\n", p->p_num_players);
	for(i = cnt = 0; i < 7 && cnt < p->p_num_players; i++) {
		int me;

		if ( p->p_player[0].name[0] == '\0' ) {
			continue;
		}

		me = !strncmp(wa->nick, p->p_player[i].name, sizeof(wa->nick));
		printf(" [%u] %.*s%s\n", i, 17, p->p_player[i].name,
			(me) ? " (*)" : "");
		if ( me )
			wa->playerid = i;
		cnt++;
	}
	printf("\n");

	return 1;
}

static int wa__player_join(wa_t wa, const struct wa_hdr *r)
{
	printf("Player join\n");
	hex_dump(r->data, r->h_len - sizeof(*r), 0);
	return 1;
}

static int wa__team_list(wa_t wa, const struct wa_hdr *r)
{
	const struct wa_team_list *t = (struct wa_team_list *)r;
	//printf("Team list %zu bytes\n", r->h_len - sizeof(*r));
	//printf("struct is %zu bytes\n", sizeof(*t) - sizeof(t->hdr));
	printf("Team(%u): (player=%u) %s\n",
		t->slot, t->playerid, t->name);
	//printf("  pad0 = %.2x\n", t->pad0);
	//printf("  pad1 = %.2x\n", t->pad1);
	//printf("  pad2 = %.2x\n", t->pad2);
	printf("  color = %u\n", t->color);
	printf("  soundbank = %s\n", t->soundbank);
	printf("  fanfare2 = %s\n", t->fanfare);
	printf("  fanfare2 = %s\n", t->fanfare2);
	//hex_dump(t->pad, sizeof(t->pad), 0);
	printf("\n");
	return 1;
}

static int wa__kick(wa_t wa, const struct wa_hdr *r)
{
	printf("You were kicked\n");
	return 1;
}

static int wa__start_game(wa_t wa, const struct wa_hdr *r)
{
	const struct wa_start_game *s = (struct wa_start_game *)r;

	printf("Game starting!\n");
	printf(" - pad0 = 0x%.2x\n", s->pad0);
	printf(" - logic_seed = 0x%.8x\n", s->logic_seed);
	printf(" - game_ver = %u\n", s->logic_seed);
	printf("\n");

	for(wa->frame = 1; wa->frame <= 0x1a; wa->frame++) {
		struct {
			struct wa_hdr hdr;
			uint8_t pad0;
			uint32_t flag;
			uint16_t status;
		}__attribute__((packed)) pkt;

		pkt.hdr.h_chan = WORMS_CHAN_GAME;
		pkt.hdr.h_unknown = 0;
		pkt.hdr.h_len = sizeof(pkt);
		pkt.hdr.h_command = 0x1;
		pkt.hdr.h_frame = wa->frame;
		pkt.pad0 = 0;
		pkt.flag = 0x0ac00000;
		pkt.status = (wa->frame - 1) * 4;
		hex_dump(&pkt, sizeof(pkt), 0);
		if ( !wa__send(wa, &pkt, sizeof(pkt)) )
			return 0;
	}

	return 1;
}

static int wa__default_scheme(wa_t wa, const struct wa_hdr *r)
{
	const struct wa_default_scheme *s = (struct wa_default_scheme *)r;
	static const char * const tbl[] = {
		[1] = "Beginner",
		[2] = "Intermediate",
		[3] = "Pro",
		[6] = "Artillery",
		[7] = "Classic",
		[8] = "Armageddon",
		[9] = "The Darkside",
		[11] = "Retro",
		[12] = "Strategic",
		[14] = "Sudden Sinking",
		[15] = "Tournament",
		[16] = "Blast Zone",
		[17] = "The Full Wormage",

	};
	const char *name;

	if ( s->s_scheme < sizeof(tbl)/sizeof(*tbl) && tbl[s->s_scheme] )
		name = tbl[s->s_scheme];
	else
		name = tbl[2];
	printf("Default scheme: %s\n", name);

	return 1;
}

static int wa__random_map(wa_t wa, const struct wa_hdr *r)
{
	const struct wa_random_map *seed = (struct wa_random_map *)r;
	printf("Random map: 0x%.8x : 0x%.8x\n", seed->seed1, seed->seed2);
	return 1;
}

void wa_exit_mainloop(wa_t wa)
{
	wa->go = 0;
}

static int wa__dispatch_lobby(wa_t wa, const struct wa_hdr *r)
{
	switch(r->h_command) {
	case WORMS_SERVER_CHAT:
		wa__chat(wa, r);
		break;
	case WORMS_SERVER_PLAYER_LIST:
		wa__player_list(wa, r);
		break;
	case WORMS_SERVER_PLAYER_JOIN:
		wa__player_join(wa, r);
		break;
	case WORMS_SERVER_READY:
		wa__ready(wa, r);
		break;
	case WORMS_SERVER_TEAM_LIST:
		wa__team_list(wa, r);
		break;
	case WORMS_SERVER_KICK:
		wa__kick(wa, r);
		break;
	case WORMS_SERVER_START_GAME:
		wa__start_game(wa, r);
		break;
	case WORMS_SERVER_DEFAULT_SCHEME:
		wa__default_scheme(wa, r);
		break;
	case WORMS_SERVER_RANDOM_MAP:
		wa__random_map(wa, r);
		break;
	default:
		if ( wa->frame ) {
			((struct wa_hdr *)r)->h_command = 0x25;
			((struct wa_hdr *)r)->data[2] = 0x1;
			if ( !wa__send(wa, r, r->h_len) )
				return 0;
			printf("ECHO ONE; ");
			wa->frame = 0;
		}
		printf("Unknown lobby cmd: 0x%.2x len=%u\n",
			r->h_command, r->h_len);
		hex_dump(r->data, r->h_len - sizeof(*r), 0);
		break;
	}

	return 1;
}

static int wa__dispatch_game(wa_t wa, const struct wa_hdr *r)
{
	switch(r->h_command) {
	case 0:
		if ( r->h_frame == 0x1b ) {
			/* echo it back */
			if ( !wa__send(wa, r, r->h_len) )
				return 0;
			wa->frame++;
			printf("ECHO: ");
		}
	default:
		printf("Unknown game cmd: 0x%.2x frame=%.2x len=%u\n",
			r->h_command, r->h_frame, r->h_len);
		hex_dump(r->data, r->h_len - sizeof(*r), 0);
		break;
	}
	return 1;
}

int wa_mainloop(wa_t wa)
{
	const struct wa_hdr *r;
	for(;wa->go;) {
		r = wa__next_msg(wa);
		if ( NULL == r )
			return 0;
		switch(r->h_chan) {
		case WORMS_CHAN_LOBBY:
			wa__dispatch_lobby(wa, r);
			break;
		case WORMS_CHAN_GAME:
			wa__dispatch_game(wa, r);
			break;
		default:
			printf("Unknown chan: 0x%.2x msg=0x%.2x len=%u\n",
				r->h_chan, r->h_command, r->h_len);
			hex_dump(r->data, r->h_len - sizeof(*r), 0);
			break;
		}
	}

	return 1;
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
	buf.hdr.h_chan = 1;
	buf.hdr.h_unknown = 0;
	buf.hdr.h_len = len + sizeof(buf.hdr);
	buf.hdr.h_command = WORMS_SERVER_CHAT;
	printf(">>> %.*s\n", (int)sizeof(buf.msg), buf.msg);
	return wa__send(wa, &buf, buf.hdr.h_len);
}

int wa_ready(wa_t wa, int ready)
{
	struct wa_ready rdy;

	memset(&rdy, 0, sizeof(rdy));
	rdy.r_hdr.h_chan = 1;
	rdy.r_hdr.h_len = sizeof(rdy);
	rdy.r_hdr.h_command = WORMS_SERVER_READY;
	rdy.r_playerid = wa->playerid;
	rdy.r_ready = !!ready;

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
