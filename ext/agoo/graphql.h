// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_GRAPHQL_H__
#define __AGOO_GRAPHQL_H__

#include <stdbool.h>
#include <stdint.h>

#include "err.h"
#include "text.h"

typedef enum {
    GQL_OBJECT		= (int8_t)1,
    GQL_FRAG		= (int8_t)2,
    GQL_INPUT		= (int8_t)3,
    GQL_UNION		= (int8_t)4,
    GQL_INTERFACE	= (int8_t)5,
    GQL_ENUM		= (int8_t)6,
    GQL_SCALAR		= (int8_t)7,
    // LIST
    // NON_NULL
} gqlKind;

struct _gqlType;
struct _gqlValue;
struct _gqlLink;
struct _gqlField;
struct _Req;

// Used for references to implemenation entities.
typedef void*	gqlRef;
typedef struct _gqlQuery {
    struct _gqlQuery	*next;
    struct _gqlValue	*result;
    // TBD data about a query, mutation, or subscription
} *gqlQuery;

typedef struct _gqlKeyVal {
    const char		*key;
    struct _gqlValue	*value;
} *gqlKeyVal;

// Resolve field on a target to a child reference.
typedef gqlRef			(*gqlResolveFunc)(gqlRef target, const char *fieldName, gqlKeyVal *args);

// Coerce an implemenation reference into a gqlValue.
typedef struct _gqlValue*	(*gqlCoerceFunc)(gqlRef ref, struct _gqlType *type);

typedef void			(*gqlIterCb)(gqlRef ref, gqlQuery query);
typedef void			(*gqlIterateFunc)(gqlRef ref, gqlIterCb cb, gqlQuery query);

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
    struct _gqlType	*type; // return type
    const char		*desc;
    const char		*reason; // deprecationReason
    gqlArg		args;
    gqlResolveFunc	resolve;
    bool		required;
    bool		list;
    bool		not_empty; // list can not be empty, member is required
    bool		deprecated;
} *gqlField;

typedef struct _gqlType {
    const char	*name;
    const char	*desc;
    Text	(*to_json)(Text text, struct _gqlValue *value, int indent, int depth);
    gqlKind	kind;
    bool	locked; // set by app
    bool	core;
    union {
	struct { // Objects, Fragments, interfaces, and input_objects
	    gqlField		fields;
	    union {
		struct _gqlType	**interfaces; // Objects, null terminated array if not NULL.
		struct _gqlType	*on;          // Fragment
	    };
	};
	struct _gqlType		**utypes;     // Union, null terminated array if not NULL.
	const char		**choices;    // Used in enums, null terminated array if not NULL.
	// Returns error code. Only for scalars.
	struct {
	    int			(*coerce)(Err err, struct _gqlValue *src, struct _gqlType *type);
	    void		(*destroy)(struct _gqlValue *value);
	};
    };
} *gqlType;

extern int	gql_init(Err err);
extern void	gql_destroy(); // clear out all

extern gqlType	gql_type_create(Err err, const char *name, const char *desc, bool locked, gqlType *interfaces);
extern gqlType	gql_fragment_create(Err err, const char *name, const char *desc, bool locked, gqlType on);
extern gqlType	gql_input_create(Err err, const char *name, const char *desc, bool locked);
extern gqlType	gql_interface_create(Err err, const char *name, const char *desc, bool locked);

extern gqlField	gql_type_field(Err		err,
			       gqlType		type,
			       const char	*name,
			       gqlType		return_type,
			       const char	*desc,
			       bool 		required,
			       bool 		list,
			       bool 		not_empty,
			       gqlResolveFunc	resolve);

extern gqlArg	gql_field_arg(Err 		err,
			      gqlField 		field,
			      const char 	*name,
			      gqlType 		type,
			      const char 	*desc,
			      struct _gqlValue	*def_value,
			      bool 		required);

// TBD maybe create then add fields
// TBD same with op? create then add args

extern gqlType	gql_union_create(Err err, const char *name, const char *desc, bool locked, gqlType *types);
extern gqlType	gql_enum_create(Err err, const char *name, const char *desc, bool locked, const char **choices); // NULL terminated choices
extern gqlType	gql_scalar_create(Err err, const char *name, const char *desc, bool locked);

extern int	gql_type_set(Err err, gqlType type);
extern gqlType	gql_type_get(const char *name);
extern void	gql_type_destroy(gqlType type);

extern Text	gql_type_sdl(Text text, gqlType type, bool comments);
extern Text	gql_schema_sdl(Text text, bool with_desc, bool all);

extern Text	gql_object_to_json(Text text, struct _gqlValue *value, int indent, int depth);
extern Text	gql_object_to_graphql(Text text, struct _gqlValue *value, int indent, int depth);
extern Text	gql_union_to_text(Text text, struct _gqlValue *value, int indent, int depth);
extern Text	gql_enum_to_text(Text text, struct _gqlValue *value, int indent, int depth);

extern void	gql_dump_hook(struct _Req *req);
extern void	gql_eval_hook(struct _Req *req);

#endif // __AGOO_GRAPHQL_H__

