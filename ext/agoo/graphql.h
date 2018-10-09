// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_GRAPHQL_H__
#define __AGOO_GRAPHQL_H__

#include <stdbool.h>
#include <stdint.h>

#include "err.h"
#include "text.h"

typedef enum {
    GQL_OBJECT	= 'O',
    GQL_UNION	= 'U',
    GQL_ENUM	= 'E',
    GQL_FRAG	= 'F',
    GQL_SCALAR	= 'S',

/*
    // scalars
    GQL_INT	= 'i',
    GQL_I64	= 'I',
    GQL_FLOAT	= 'f',
    GQL_STRING	= 's',
    GQL_STR16	= 'S', // string less than 16 characters
    GQL_BOOL	= 'b',
    GQL_TIME	= 't',
    GQL_ID	= 'I',
    GQL_UUID	= 'u',
    GQL_URL	= 'L',
*/
} gqlKind;

struct _gqlType;
struct _gqlValue;

typedef struct _gqlArg {
    struct _gqlArg	*next;
    const char		*name;
    const char		*desc;
    struct _gqlType	*type;
    struct _gqlValue	*default_value;
    bool		required;
} *gqlArg;

typedef struct _gqlField {
    struct _gqlField	*next;
    const char		*name;
    struct _gqlType	*type;
    const char		*desc;
    gqlArg		args;
    // TBD should all field have an eval function returns some data item (type?) or just fill in text
    //   if just filling in text then what about lists unless func build and takes args for children
    //   if to gqlValues then other output options are possible (does this matter?)
    bool		required;
    bool		list;
    bool		operation;
} *gqlField;

typedef struct _gqlType {
    const char	*name;
    const char	*desc;
    Text	(*to_text)(Text text, struct _gqlValue *value);
    gqlKind	kind;
    bool	locked; // set by app
    union {
	struct { // Objects and Fragments
	    gqlField		fields;
	    struct _gqlType	**interfaces; // Objects, null terminated array if not NULL.
	};
	struct _gqlType		*utypes;     // Union, null terminated array if not NULL.
	const char		**choices;   // Used in enums, null terminated array if not NULL.
	// Returns error code. Only for scalars.
	int			(*coerce)(Err err, struct _gqlValue *src, struct _gqlType *type);
    };
} *gqlType;

extern int	gql_init(Err err);
extern void	gql_destroy(); // clear out all

extern int	gql_field_create(Err err, const char *name, gqlType type, const char *desc, bool required, bool list);
extern int	gql_arg_create(Err err, const char *name, gqlType type, const char *desc, struct _gqlValue *def_value, gqlArg next);
extern int	gql_op_create(Err err, const char *name, gqlType return_type, const char *desc, gqlArg args); // linked list of args
extern int	gql_type_create(Err err, const char *name, const char *desc, bool locked, gqlField fields, gqlType interfaces);
extern int	gql_union_create(Err err, const char *name, const char *desc, bool locked, gqlType types);
extern int	gql_enum_create(Err err, const char *name, const char *desc, bool locked, const char **choices); // NULL terminated choices
extern int	gql_fragment_create(Err err, const char *name, const char *desc, bool locked, gqlField fields);
extern int	gql_scalar_create(Err err, const char *name, const char *desc, bool locked);

extern int	gql_type_set(Err err, gqlType type);
extern gqlType	gql_type_get(const char *name);
extern void	gql_type_destroy(gqlType type);
extern Text	gql_type_text(Text text, gqlType value, int indent);

#endif // __AGOO_GRAPHQL_H__
