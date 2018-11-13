// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_UPGRADED_H
#define AGOO_UPGRADED_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>

struct _Con;
struct _Subject;

typedef struct _Upgraded {
    struct _Upgraded	*next;
    struct _Upgraded	*prev;
    struct _Con		*con;
    atomic_int		pending;
    atomic_int		ref_cnt;
    struct _Subject	*subjects;

    void		*ctx;
    void		*wrap;
    void		*env;

    bool		on_empty;
    bool		on_close;
    bool		on_shut;
    bool		on_msg;
    bool		on_error;
    void		(*on_destroy)(struct _Upgraded *up);
} *Upgraded;

extern Upgraded	upgraded_create(struct _Con *c, void * ctx, void *env);

extern void	upgraded_release(Upgraded up);
extern void	upgraded_release_con(Upgraded up);

extern void	upgraded_ref(Upgraded up);

extern void	upgraded_add_subject(Upgraded up, struct _Subject *subject);
extern void	upgraded_del_subject(Upgraded up, struct _Subject *subject);
extern bool	upgraded_match(Upgraded up, const char *subject);

extern bool	upgraded_write(Upgraded up, const char *message, size_t mlen, bool bin, bool inc_ref);
extern void	upgraded_subscribe(Upgraded up, const char *subject, int slen, bool inc_ref);
extern void	upgraded_unsubscribe(Upgraded up, const char *subject, int slen, bool inc_ref);
extern void	upgraded_close(Upgraded up, bool inc_ref);
extern int	upgraded_pending(Upgraded up);

#endif // AGOO_UPGRADED_H
