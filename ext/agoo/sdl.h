// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_SDL_H
#define AGOO_SDL_H

#include "err.h"

struct _gqlVar;

extern int		sdl_parse(agooErr err, const char *str, int len);

// Parse a execution definition.
extern struct _gqlDoc*	sdl_parse_doc(agooErr err, const char *str, int len, struct _gqlVar *vars);

extern gqlVar	gql_op_var_create(agooErr err, const char *name, gqlType type, gqlValue value);

#endif // AGOO_SDL_H
