#ifndef _WA_PROTOCOK_H
#define _WA_PROTOCOL_H

#define WORMS_CHAN_LOBBY		0x01
#define WORMS_CHAN_GAME			0x02

struct wa_hdr {
	uint8_t h_chan;
	uint8_t h_unknown;
	uint16_t h_len;
	uint8_t h_command;
	uint8_t h_frame;
	uint8_t data[0];
}__attribute__((packed));

#define WORMS_COMMAND_LOGIN		0x04
struct wa_login {
	struct wa_hdr l_hdr;
	char l_name[17];
	char l_gamename[17];
	uint8_t l_pad0[24];
	uint8_t l_vers[3];
	uint8_t l_pad1[61];
}__attribute__((packed));

#define WORMS_COMMAND_LOGIN2		0x05
struct wa_login2 {
	struct wa_hdr l_hdr;
	char l_name[17];
	uint8_t l_pad0[49];
	uint8_t l_country;
	uint8_t l_pad1[41];
}__attribute__((packed));

#define WORMS_SERVER_CHAT		0x00
#define WORMS_SERVER_LOGIN_OK		0x08
#define WORMS_SERVER_PLAYER_LIST	0x0b
struct wa_playerlist {
	struct wa_hdr p_hdr;
	uint8_t p_pre[6];
	struct {
		char name[17];
		uint8_t stuff[17];
		uint8_t maybestuff[14];
		uint8_t pad0[60];
		uint16_t prev;
		uint16_t pad1;
		uint8_t country;
		uint8_t pad2[3];
		uint8_t flag1;
		uint8_t flag2;
		uint16_t pad3;
	}__attribute__((packed)) p_player[7];
	uint8_t p_pad0[0x2d0];
	uint16_t p_num_players;
	uint16_t p_pad1;
}__attribute__((packed));

#define WORMS_SERVER_PLAYER_JOIN	0x0e

#define WORMS_SERVER_READY		0x0f
struct wa_ready {
	struct wa_hdr r_hdr;
	uint16_t r_pad0;
	uint32_t r_ready;
	uint32_t r_playerid;
}__attribute__((packed));

#define WORMS_SERVER_TEAM_LIST		0x0c
struct wa_team_list {
	struct wa_hdr		hdr;
	uint16_t		pad0;
	uint16_t		slot;
	uint16_t		pad1;
	uint8_t			playerid;
	uint8_t			color;
	uint8_t			pad2;
	char			name[17];
	char			soundbank[32];
	uint16_t		soundbank_flag; /* ? */
	char			fanfare[30];
	uint8_t			pad3;
	char			fanfare2[30];
	uint8_t			pad4[41];

	uint8_t			worm[8][17];

	uint8_t			pad[3160];
}__attribute__((packed));

#define WORMS_SERVER_TEAM_REMOVE	0x10
#define WORMS_SERVER_KICK		0x1b
#define WORMS_SERVER_TEAM_ADD		0x1a
#define WORMS_SERVER_START_GAME		0x1c
struct wa_start_game {
	struct wa_hdr		hdr;
	uint16_t		pad0;
	uint32_t		logic_seed;
#define WORMS_GAME_START_MAGIC	"GSAW"
	char			magic[4];
	uint32_t		game_ver;
}__attribute__((packed));

#define WORMS_SERVER_DEFAULT_SCHEME	0x1f
struct wa_default_scheme {
	struct wa_hdr s_hdr;
	uint16_t s_pad0;
	uint32_t s_scheme;
}__attribute__((packed));

#define WORMS_SERVER_RANDOM_MAP		0x21
struct wa_random_map {
	struct wa_hdr r_hdr;
	uint16_t pad0;
	uint32_t flag1;
	uint32_t flag2;
	uint32_t seed1;
	uint32_t seed2;
	uint32_t flags[6];
}__attribute__((packed));

#endif /* _WA_PROTOCOL_H */
