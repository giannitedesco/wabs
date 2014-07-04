#ifndef _WA_PROTOCOK_H
#define _WGT_FORMAT_H

struct wgt_hdr {
	char		wgt_sig[3];
	char		wgt_pad0[2];
	uint8_t		wgt_num_teams;
	uint8_t		wgt_util_cheats;
	uint8_t		wgt_weap_cheats;
	uint8_t		wgt_game_cheats;
	uint8_t		wgt_indestructable_terrain;
	uint8_t		wgt_pad2;
}__attribute__((packed));

struct wgt_team {
	uint8_t		t_name[17];
	uint8_t		t_worm[8][17];
	uint8_t		t_player;
	char		t_soundbank[32];
	uint8_t		t_soundbank_flag;
	char		t_fanfare[32];
	uint8_t		t_fanfare_flag;
	uint8_t		t_grave;

	/* if t_grave == 0xff:
	 * 32 byte name
	 * 1024 byte palette
	 * 768 byte 32 rows of 24 pixels
	 */

	uint8_t		t_weapon;

	uint32_t	t_stats[10];

	/* attempts, medal */
	uint32_t	t_missions[33][2];

	char		t_flag[32];

	/* bgr0 palette */
	uint8_t		t_flag_palette[256][4];
	uint8_t		t_flag_bitmap[17][20];

	uint8_t		t_deathmatch_rank;
	uint32_t	t_training_times[6];

	uint8_t		t_pad0[40];
	uint8_t		t_training_medals[6];

	uint8_t		t_pad1[10];
	uint8_t		t_pad2[7][4];

	uint8_t		t_pad3;
}__attribute__((packed));

#endif /* _WGT_FORMAT_H */
