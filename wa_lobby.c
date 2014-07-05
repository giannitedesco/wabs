#define _GNU_SOURCE
#include <worms/worms.h>
#include <worms/wa.h>
#include <worms/wgt.h>
#include <worms/wa-protocol.h>

#include "wa-internal.h"

static int wa__chat(wa_t wa, const struct wa_hdr *r)
{
	printf("Chat: %.*s\n", (int)(r->len - sizeof(*r)), r->data);
	return 1;
}

static int wa__ready(wa_t wa, const struct wa_hdr *r)
{
	const struct wa_ready *rdy = (struct wa_ready *)r;
	printf("Player %u %sready!\n",
		rdy->playerid, (rdy->ready) ? "" : "not ");
	return 1;
}

static int wa__player_list(wa_t wa, const struct wa_hdr *r)
{
	const struct wa_playerlist *p = (struct wa_playerlist *)r;
	unsigned int i, cnt;

	printf("Player list: %u players\n", p->num_players);
	for(i = cnt = 0; i < 7 && cnt < p->num_players; i++) {
		int me;

		if ( p->player[0].name[0] == '\0' ) {
			continue;
		}

		me = !strncmp(wa->nick, p->player[i].name, sizeof(wa->nick));
		printf(" [%u] %.*s%s\n", i, 17, p->player[i].name,
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
	hex_dump(r->data, r->len - sizeof(*r), 0);
	return 1;
}

static int wa__team_list(wa_t wa, const struct wa_hdr *r)
{
	const struct wa_team_list *t = (struct wa_team_list *)r;
	//printf("Team list %zu bytes\n", r->len - sizeof(*r));
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

		pkt.hdr.chan = WORMS_CHAN_GAME;
		pkt.hdr.unknown = 0;
		pkt.hdr.len = sizeof(pkt);
		pkt.hdr.command = 0x1;
		pkt.hdr.frame = wa->frame;
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

	if ( s->scheme < sizeof(tbl)/sizeof(*tbl) && tbl[s->scheme] )
		name = tbl[s->scheme];
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

int wa__dispatch_lobby(wa_t wa, const struct wa_hdr *r)
{
	switch(r->command) {
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
			((struct wa_hdr *)r)->command = 0x25;
			((struct wa_hdr *)r)->data[2] = 0x1;
			if ( !wa__send(wa, r, r->len) )
				return 0;
			printf("ECHO ONE; ");
			wa->frame = 0;
		}
		printf("Unknown lobby cmd: 0x%.2x len=%u\n",
			r->command, r->len);
		hex_dump(r->data, r->len - sizeof(*r), 0);
		break;
	}

	return 1;
}
