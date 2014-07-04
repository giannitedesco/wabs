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
	}

	memset(&l2, 0, sizeof(l2));
	l2.l_hdr.h_chan = 1;
	l2.l_hdr.h_len = sizeof(l2);
	l2.l_hdr.h_command = WORMS_COMMAND_LOGIN2;

	snprintf(l2.l_name, sizeof(l2.l_name), "%s", nick);
	l2.l_country = 0xf;

	if ( !wa__send(wa, &l2, sizeof(l2)) )
		return 0;

	free(wa->nick);
	wa->nick = strdup(nick);
	if ( NULL == wa->nick ) {
		fprintf(stderr, "%s: calloc: %s\n", cmd, sys_err());
		return 0;
	}
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
		if ( p->p_player[0].name[0] == '\0' ) {
			continue;
		}
		printf(" [%u] %.*s\n", i, 17, p->p_player[i].name);
		cnt++;
	}

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
	printf("Team list %u\n", r->h_len);
	//hex_dump(r, r->h_len, 0);
	return 1;
}

static int wa__kick(wa_t wa, const struct wa_hdr *r)
{
	printf("You were kicked\n");
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

int wa_mainloop(wa_t wa)
{
	const struct wa_hdr *r;
	for(;wa->go;) {
		r = wa__next_msg(wa);
		if ( NULL == r )
			return 0;
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
		case WORMS_SERVER_DEFAULT_SCHEME:
			wa__default_scheme(wa, r);
			break;
		case WORMS_SERVER_RANDOM_MAP:
			wa__random_map(wa, r);
			break;
		default:
			printf("Unknown msg: 0x%.2x len=%u\n",
				r->h_command, r->h_len);
			hex_dump((void *)r, r->h_len, 0);
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

	len = snprintf(buf.msg, sizeof(buf.msg), "%s:%s:%s:%s\n",
			type, wa->nick,
			(to) ? to : "ALL", msg);
	buf.hdr.h_chan = 1;
	buf.hdr.h_unknown = 0;
	buf.hdr.h_len = len + sizeof(buf.hdr);
	buf.hdr.h_command = WORMS_SERVER_CHAT;
	return wa__send(wa, &buf, sizeof(buf.hdr) + len);
}

int wa_ready(wa_t wa, int ready)
{
	struct wa_ready rdy;

	memset(&rdy, 0, sizeof(rdy));
	rdy.r_hdr.h_chan = 1;
	rdy.r_hdr.h_len = sizeof(rdy);
	rdy.r_hdr.h_command = WORMS_SERVER_READY;
	rdy.r_playerid = 1;
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
		free(wa->nick);
		close(wa->s);
		free(wa);
	}
}