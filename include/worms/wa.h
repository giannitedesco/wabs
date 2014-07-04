#ifndef _WA_PROTOCOK_H
#define _WORMS_CONNECTION_H

typedef struct _wa *wa_t;

wa_t wa_connect(uint32_t ip, uint16_t port, const char *nick);
void wa_close(wa_t wa);

int wa_ready(wa_t wa, int ready);

#define CHAT_GLOBAL "GLB"
#define CHAT_SYS "SYS"
#define CHAT_PRIV "PRV"
int wa_chat(wa_t wa, const char *type, const char *to, const char *msg);

int wa_mainloop(wa_t wa);
void wa_exit_mainloop(wa_t wa);

const char *wa_get_nick(wa_t wa);

#endif /* _WORMS_CONNECTION_H */
