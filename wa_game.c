#define _GNU_SOURCE
#include <worms/worms.h>
#include <worms/wa.h>
#include <worms/wgt.h>
#include <worms/wa-protocol.h>

#include "wa-internal.h"

int wa__dispatch_game(wa_t wa, const struct wa_frame *f)
{
	/* yeah, frame id is probably 16bits, and the rest
	 * are some flags or something... Maybe even a bitmap,
	 * see 1, 2 and 4 set there.
	*/
	if ( f->playerid == 0 && f->frame == 0x0200001b ) {
		/* echo it back */
		if ( !wa__send(wa, f, f->len) )
			return 0;
		wa->frame++;
	}
	printf("Frame: playerid=%u frame=0x%.8x len=%zu\n",
		f->playerid, f->frame, f->len - sizeof(*f));
	hex_dump(f->data, f->len - sizeof(*f), 0);
	return 1;
}
