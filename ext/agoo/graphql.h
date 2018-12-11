// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_GRAPHQL_H
#define AGOO_GRAPHQL_H

#include <stdbool.h>
#include <stdint.h>

#include "err.h"
#include "text.h"

typedef enum {
    GQL_UNDEF		= (int8_t)0, // not defined yet
    GQL_OBJECT		= (int8_t)1,
    GQL_FRAG		= (int8_t)2,
    GQL_INPUT		= (int8_t)3,
    GQL_UNION		= (int8_t)4,
    GQL_INTERFACE	= (int8_t)5,
    GQL_ENUM		= (int8_t)6,
    GQL_SCALAR		= (int8_t)7,
    GQL_LIST		= (int8_t)8,
} gqlKind;

struct _agooCon;
struct _agooReq;
struct _gqlDirUse;
struct _gqlField;
struct _gqlLink;
struct _gqlLink;
struct _gqlType;
struct _gqlValue;

// Used for references to implemenation entities.
typedef void*	gqlRef;

typedef struct _gqlQuery {
    struct _gqlQuery	*next;
    struct _gqlValue	*result;
    struct _gqlDirUse	*dir;
    // TBD data about a query, mutation, or subscription
} *gqlQuery;

typedef struct _gqlKeyVal {
    const char		*key;
    struct _gqlValue	*value;
} *gqlKeyVal;

typedef struct _gqlStrLink {
    struct _gqlStrLink	*next;
    char		*str;
} *gqlStrLink;

typedef struct _gqlTypeLink {
    struct _gqlTypeLink	*next;
    struct _gqlType	*type;
} *gqlTypeLink;

// Resolve field on a target to a child reference.
typedef gqlRef			(*gqlResolveFunc)(gqlRef target, const char *fieldName, gqlKeyVal *args);

// Coerce an implemenation reference into a gqlValue.
typedef struct _gqlValue*	(*gqlCoerceFunc)(gqlRef ref, struct _gqlType *type);

typedef void			(*gqlIterCb)(gqlRef ref, gqlQuery query);
typedef void			(*gqlIterateFunc)(gqlRef ref, gqlIterCb cb, gqlQuery query);

typedef struct _gqlEnumVal {
    struct _gqlEnumVal	*next;
    const char		*value;
    const char		*desc;
    struct _gqlDirUse	*dir;
} *gqlEnumVal;

typedef struct _gqlArg {
    struct _gqlArg	*next;
    const char		*name;
    const char		*desc;
    struct _gqlType	*type;
    struct _gqlValue	*default_value;
    struct _gqlDirUse	*dir;
    bool		required;
} *gqlArg;

typedef struct _gqlField {
    struct _gqlField	*next;
    const char		*name;
    struct _gqlType	*type; // return type
    const char		*desc;
    gqlArg		args;
    struct _gqlDirUse	*dir;
    struct _gqlValue	*default_value;
    gqlResolveFunc	resolve;
    bool		required;
} *gqlField;

typedef struct _gqlDir {
    struct _gqlDir	*next;
    const char		*name;
    const char		*desc;
    gqlArg		args;
    gqlStrLink		locs; // location names
    bool		defined;
} *gqlDir;

typedef struct _gqlDirUse {
    struct _gqlDirUse	*next;
    gqlDir		dir;
    struct _gqlLink	*args;
} *gqlDirUse;

typedef struct _gqlType {
    const char	*name;
    const char	*desc;
    agooText	(*to_json)(agooText text, struct _gqlValue *value, int indent, int depth);
    agooText	(*to_sdl)(agooText text, struct _gqlValue *value, int indent, int depth);
    gqlDirUse	dir;
    gqlKind	kind;
    bool	core;
    union {
	struct { // Objects, Fragments, interfaces, and input_objects
	    gqlField		fields;
	    union {
		gqlTypeLink	interfaces;   // Types
		struct _gqlType	*on;          // Fragment
	    };
	};
	gqlTypeLink		types;	      // Union
	gqlEnumVal		choices;      // Enums
	// Returns error code. Only for scalars.
	struct {
	    int			(*coerce)(agooErr err, struct _gqlValue *src, struct _gqlType *type);
	    void		(*destroy)(struct _gqlValue *value);
	};
	struct { // List types
	    struct _gqlType	*base;
	    bool		not_empty;
	};
    };
} *gqlType;

extern int	gql_init(agooErr err);
extern void	gql_destroy(); // clear out all

extern gqlType	gql_type_create(agooErr err, const char *name, const char *desc, size_t dlen, gqlTypeLink interfaces);
extern gqlType	gql_assure_type(agooErr err, const char *name);

extern gqlType	gql_fragment_create(agooErr err, const char *name, const char *desc, size_t dlen, const char *on);
extern gqlType	gql_input_create(agooErr err, const char *name, const char *desc, size_t dlen);
extern gqlType	gql_interface_create(agooErr err, const char *name, const char *desc, size_t dlen);

extern gqlField	gql_type_field(agooErr		err,
			       gqlType		type,
			       const char	*name,
			       gqlType		return_type,
			       struct _gqlValue	*default_value,
			       const char	*desc,
			       size_t		dlen,
			       bool 		required,
			       gqlResolveFunc	resolve);

extern gqlArg	gql_field_arg(agooErr 		err,
			      gqlField 		field,
			      const char 	*name,
			      gqlType	 	type,
			      const char 	*desc,
			      size_t		dlen,
			      struct _gqlValue	*def_value,
			      bool 		required);

// TBD maybe create then add fields
// TBD same with op? create then add args

extern gqlType	gql_scalar_create(agooErr err, const char *name, const char *desc, size_t dlen);

extern gqlDir	gql_directive_create(agooErr err, const char *name, const char *desc, size_t dlen);
extern int	gql_directive_on(agooErr err, gqlDir d, const char *on, int len);
extern gqlArg	gql_dir_arg(agooErr 		err,
			    gqlDir 		dir,
			    const char 		*name,
			    gqlType 		type,
			    const char	 	*desc,
			    size_t		dlen,
			    struct _gqlValue	*def_value,
			    bool 		required);
extern gqlDir	gql_directive_get(const char *name);

extern gqlDirUse	gql_dir_use_create(agooErr err, const char *name);
extern int		gql_dir_use_arg(agooErr err, gqlDirUse use, const char *key, struct _gqlValue *value);

extern gqlType		gql_union_create(agooErr err, const char *name, const char *desc, size_t dlen);
extern int		gql_union_add(agooErr err, gqlType type, gqlType member);

extern gqlType		gql_enum_create(agooErr err, const char *name, const char *desc, size_t dlen);
extern gqlEnumVal	gql_enum_append(agooErr err, gqlType type, const char *value, size_t len, const char *desc, size_t dlen);

extern gqlType	gql_assure_list(agooErr err, gqlType base, bool not_empty);

extern int	gql_type_set(agooErr err, gqlType type);
extern gqlType	gql_type_get(const char *name);
extern void	gql_type_destroy(gqlType type);

extern agooText	gql_type_sdl(agooText text, gqlType type, bool comments);
extern agooText	gql_directive_sdl(agooText text, gqlDir dir, bool comments);
extern agooText	gql_schema_sdl(agooText text, bool with_desc, bool all);

extern agooText	gql_object_to_graphql(agooText text, struct _gqlValue *value, int indent, int depth);
extern agooText	gql_union_to_text(agooText text, struct _gqlValue *value, int indent, int depth);
extern agooText	gql_enum_to_text(agooText text, struct _gqlValue *value, int indent, int depth);

extern void	gql_dump_hook(struct _agooReq *req);
extern void	gql_eval_hook(struct _agooReq *req);

extern int	gql_validate(agooErr err);

#endif // AGOO_GRAPHQL_H
