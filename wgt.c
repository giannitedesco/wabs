#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <worms/worms.h>
#include <worms/wgt-format.h>
#include <worms/wgt.h>

static const void *mapfile(int fd, size_t *len)
{
	struct stat st;
	const char *map;

	*len = 0;

	if ( fstat(fd, &st) ) {
		fprintf(stderr, "%s: fstat: %s\n", cmd, sys_err());
		return NULL;
	}

	map = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if ( map == MAP_FAILED ) {
		fprintf(stderr, "%s: mmap: %s\n", cmd, sys_err());
		return NULL;
	}

	*len = st.st_size;
	return map;
}

struct _wgt {
	const struct wgt_hdr *hdr;
	const struct wgt_team *team;
	size_t maplen;
};

static void dump(struct _wgt *w)
{
#if 0
	unsigned int i;

	printf("%u teams\n", w->hdr->wgt_num_teams);
	printf("%zu byte header\n", sizeof(*w->hdr));
	printf("%zu byte team\n", sizeof(*w->team));
	for(i = 0; i < w->hdr->wgt_num_teams; i++) {
		unsigned int j;

		if ( w->team[i].t_player )
			continue;
		//hex_dump(&w->team[i], sizeof(w->team[i]), 0);
		printf("Team: %s\n", w->team[i].t_name);
		printf("   soundbank = %s\n", w->team[i].t_soundbank);
		printf("   fanfare = %s\n", w->team[i].t_fanfare);
		printf("   grave type = 0x%.2x\n", w->team[i].t_grave);
		printf("   weapon = %u\n", w->team[i].t_weapon);
		printf("   flag = %s\n", w->team[i].t_flag);
		//hex_dump(w->team[i].t_pad, sizeof(w->team[i].t_pad), 0);
		for(j = 0; j < 8; j++) {
			printf(" - %s\n", w->team[i].t_worm[j]);
		}
	}
#endif
}

wgt_t wgt_open(const char *fn)
{
	struct _wgt *w;
	int fd;

	w = calloc(1, sizeof(*w));
	if ( NULL == w ) {
		fprintf(stderr, "%s: calloc: %s\n", cmd, sys_err());
		goto out;
	}

	fd = open(fn, O_RDONLY);
	if ( fd < 0 ) {
		fprintf(stderr, "%s: open: %s: %s\n", cmd, fn, sys_err());
		goto out_free;
	}

	w->hdr = mapfile(fd, &w->maplen);
	close(fd);
	if ( NULL == w->hdr ) {
		goto out_free;
	}

	if ( w->maplen < sizeof(*w->hdr) ) {
		fprintf(stderr, "%s: %s: too small to be a wgt file\n",
				cmd, fn);
		goto out_close;
	}

	if ( strncmp(w->hdr->wgt_sig, WGT_SIG, sizeof(w->hdr->wgt_sig)) ) {
		fprintf(stderr, "%s: %s: doesn't look like a WGT file\n",
				cmd, fn);
		goto out_close;
	}

	w->team = (void *)&w->hdr[1];
	dump(w);

	/* success */
	goto out;

out_close:
	munmap((void *)w->hdr, w->maplen);
out_free:
	free(w);
	w = NULL;
out:
	return w;
}

void wgt_close(wgt_t w)
{
	if ( w ) {
		munmap((void *)w->hdr, w->maplen);
		free(w);
	}
}
