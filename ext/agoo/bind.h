// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_BIND_H
#define AGOO_BIND_H

#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "err.h"
#include "kinds.h"

struct _agooCon;

typedef struct _agooBind {
    struct _agooBind	*next;
    int			fd;
    int			port;
    sa_family_t		family;
    union {
	struct in_addr	addr4;
	struct in6_addr	addr6;
    };
    bool		(*read)(struct _agooCon *c);
    bool		(*write)(struct _agooCon *c);
    short		(*events)(struct _agooCon *c);
    char		*name; // if set then Unix file
    char		*id;
    agooConKind		kind;
} *agooBind;

extern agooBind	agoo_bind_url(agooErr err, const char *url);
extern agooBind	agoo_bind_port(agooErr err, int port);
extern void	agoo_bind_destroy(agooBind b);

extern int	agoo_bind_listen(agooErr err, agooBind b);
extern void	agoo_bind_close(agooBind b);

#endif // AGOO_BIND_H
