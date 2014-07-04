#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

#include <worms/worms.h>
#include <worms/wa.h>
#include <worms/wa-protocol.h>

const char *cmd;

static int cmd_chat(wa_t wa, char *args)
{
	return wa_chat(wa, CHAT_GLOBAL, NULL, args);
}

static int cmd_me(wa_t wa, char *args)
{
	char buf[strlen(args) + strlen(wa_get_nick(wa)) + 2];
	snprintf(buf, sizeof(buf), "%s %s", wa_get_nick(wa), args);
	return wa_chat(wa, CHAT_SYS, NULL, buf);
}

static int cmd_priv(wa_t wa, char *args)
{
	char *sp;
	for(sp = args; !isspace(*sp) && *sp != '\0'; sp++)
		/* nothing */;
	*sp = '\0';
	return wa_chat(wa, CHAT_PRIV, args, sp + 1);
}

static int cmd_rup(wa_t wa, char *args)
{
	return wa_ready(wa, 1);
}

static wa_t wa;
static int dispatch(const char *cmd, char *args)
{
	static const struct {
		const char *cmd;
		int (*fn)(wa_t wa, char *args);
	}tbl[] = {
		{"rup", cmd_rup},
		{"me", cmd_me},
		{"msg", cmd_priv},
	};
	unsigned int i;

	for(i = 0; i < sizeof(tbl)/sizeof(tbl[0]); i++) {
		if ( !strcmp(cmd, tbl[i].cmd) )
			return tbl[i].fn(wa, args);
	}

	printf("command: %s: not found\n", cmd);
	return 1;
}

static void cmd_handler(char *cmd)
{
	int ret = 1;

	if ( NULL == cmd ) {
		wa_exit_mainloop(wa);
		return;
	}

	if ( cmd[0] == '/' ) {
		char *sp;
		add_history(cmd);

		for(sp = cmd; !isspace(*sp) && *sp != '\0'; sp++)
			/* nothing */;
		*sp = '\0';
		ret = dispatch(cmd + 1, sp + 1);
	}else if ( cmd[0] != '\0'){
		ret = cmd_chat(wa, cmd);
	}

	if ( !ret ) {
		wa_exit_mainloop(wa);
	}
}

int main(int argc, char **argv)
{
	struct in_addr addr;
	const char *name, *nick;
	char *hfile;
	int ret = EXIT_FAILURE;

	rl_initialize();
	using_history();

	if ( argc > 0 )
		cmd = argv[0];
	else
		cmd = "wa";

	if ( argc > 1 )
		name = argv[1];
	else
		name = "127.0.0.1";

	if ( argc > 2 )
		nick = argv[2];
	else
		nick = "moriarty";

	if ( asprintf(&hfile, "%s/%s", getenv("HOME"), ".wa") < 0 ) {
		fprintf(stderr, "%s: asnprintf: %s\n", cmd, sys_err());
		goto out;
	}

	if ( read_history(hfile) && errno != ENOENT ) {
		fprintf(stderr, "%s: read_history: %s\n", cmd, sys_err());
		goto out_free;
	}

	rl_callback_handler_install("worms> ", cmd_handler);

	if ( !inet_aton(name, &addr) ) {
		fprintf(stderr, "%s: inet_aton: %s\n", cmd, sys_err());
		goto out_write;
	}

	printf("Connecting to: %s 17011\n", name);
	wa = wa_connect(addr.s_addr, 17011, nick);
	if ( NULL == wa )
		goto out_write;

	if ( !wa_mainloop(wa) ) {
		goto out_close;
	}

	ret = EXIT_SUCCESS;

out_close:
	wa_close(wa);
out_write:
	if ( write_history(hfile) ) {
		fprintf(stderr, "%s: write_history: %s\n", cmd, sys_err());
	}
out_free:
	free(hfile);
out:
	rl_callback_handler_remove();
	return ret;
}
