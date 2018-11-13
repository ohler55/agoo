// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_BIND_H
#define AGOO_BIND_H

#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "err.h"
#include "kinds.h"

struct _Con;

typedef struct _Bind {
    struct _Bind	*next;
    int			fd;
    int			port;
    sa_family_t		family;
    union {
	struct in_addr	addr4;
	struct in6_addr	addr6;
    };
    ConKind		kind;
    bool		(*read)(struct _Con *c);
    bool		(*write)(struct _Con *c);
    short		(*events)(struct _Con *c);
    char		scheme[8];
    char		*name; // if set then Unix file
    char		*key;  // if set then SSL
    char		*cert;
    char		*ca;
    char		*id;
} *Bind;

extern Bind	bind_url(Err err, const char *url);
extern Bind	bind_port(Err err, int port);
extern void	bind_destroy(Bind b);

extern int	bind_listen(Err err, Bind b);
extern void	bind_close(Bind b);

#endif // AGOO_BIND_H
