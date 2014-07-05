#define _GNU_SOURCE
#include <worms/worms.h>
#include <worms/wa.h>
#include <worms/wgt.h>
#include <worms/wa-protocol.h>

#include "wa-internal.h"

int wa__dispatch_game(wa_t wa, const struct wa_hdr *r)
{
	switch(r->command) {
	case 0:
		if ( r->frame == 0x1b ) {
			/* echo it back */
			if ( !wa__send(wa, r, r->len) )
				return 0;
			wa->frame++;
			printf("ECHO: ");
		}
	default:
		printf("Unknown game cmd: 0x%.2x frame=%.2x len=%u\n",
			r->command, r->frame, r->len);
		hex_dump(r->data, r->len - sizeof(*r), 0);
		break;
	}
	return 1;
}

