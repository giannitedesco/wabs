#define _GNU_SOURCE
#include <sys/types.h>
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

int wa__wait(struct _wa *wa)
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

int wa__send(struct _wa *wa, const void *buf, size_t len)
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

int wa__recv(struct _wa *wa)
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

const struct wa_hdr *wa__next_msg(struct _wa *wa)
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
	assert(hdr->len < sizeof(wa->buf));
	assert(hdr->len >= sizeof(*hdr));
	end = wa->msg_head + hdr->len;
	while(wa->msg_tail < end) {
		//printf("reading for body %zu < %zu\n",
		//	wa->msg_tail - wa->msg_head, end - wa->msg_head);
		if ( !wa__recv(wa) )
			return NULL;
	}

	//printf("Dispatching %u bytes\n", hdr->len);
	//hex_dump(hdr, hdr->len, 0);
	wa->msg_head += hdr->len;
	return hdr;
}
