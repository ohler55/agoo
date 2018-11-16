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
    agooConKind		kind;
    bool		(*read)(struct _agooCon *c);
    bool		(*write)(struct _agooCon *c);
    short		(*events)(struct _agooCon *c);
    char		scheme[8];
    char		*name; // if set then Unix file
    char		*key;  // if set then SSL
    char		*cert;
    char		*ca;
    char		*id;
} *agooBind;

extern agooBind	bind_url(agooErr err, const char *url);
extern agooBind	bind_port(agooErr err, int port);
extern void	bind_destroy(agooBind b);

extern int	bind_listen(agooErr err, agooBind b);
extern void	bind_close(agooBind b);

#endif // AGOO_BIND_H
