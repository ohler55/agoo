// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_SDL_H
#define AGOO_SDL_H

#include "err.h"

extern int		sdl_parse(agooErr err, const char *str, int len);

// Parse a execution definition.
extern struct _gqlDoc*	sdl_parse_doc(agooErr err, const char *str, int len);

#endif // AGOO_SDL_H
