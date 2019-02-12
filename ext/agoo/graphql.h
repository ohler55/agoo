// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_GRAPHQL_H
#define AGOO_GRAPHQL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "err.h"
#include "gqlcobj.h"
#include "gqleval.h"
#include "text.h"

typedef enum {
    GQL_UNDEF		= (int8_t)0, // not defined yet
    GQL_SCHEMA		= (int8_t)1,
    GQL_OBJECT		= (int8_t)2,
    GQL_FRAG		= (int8_t)3,
    GQL_INPUT		= (int8_t)4,
    GQL_UNION		= (int8_t)5,
    GQL_INTERFACE	= (int8_t)6,
    GQL_ENUM		= (int8_t)7,
    GQL_SCALAR		= (int8_t)8,
    GQL_LIST		= (int8_t)9,
    GQL_NON_NULL	= (int8_t)10,
} gqlKind;

typedef enum {
    GQL_SCALAR_UNDEF	= (int8_t)0, // not defined yet
    GQL_SCALAR_NULL	= (int8_t)1,
    GQL_SCALAR_BOOL	= (int8_t)2,
    GQL_SCALAR_INT	= (int8_t)3,
    GQL_SCALAR_I64	= (int8_t)4,
    GQL_SCALAR_FLOAT	= (int8_t)5,
    GQL_SCALAR_STRING	= (int8_t)6,
    GQL_SCALAR_TOKEN	= (int8_t)7,
    GQL_SCALAR_VAR	= (int8_t)8,
    GQL_SCALAR_TIME	= (int8_t)9,
    GQL_SCALAR_UUID	= (int8_t)10,
    GQL_SCALAR_ID	= (int8_t)11,
    GQL_SCALAR_LIST	= (int8_t)12,
    GQL_SCALAR_OBJECT	= (int8_t)13,
} gqlScalarKind;

typedef enum {
    GQL_QUERY		= (int8_t)'Q',
    GQL_MUTATION	= (int8_t)'M',
    GQL_SUBSCRIPTION	= (int8_t)'S',
} gqlOpKind;

struct _agooCon;
struct _agooReq;
struct _gqlDirUse;
struct _gqlField;
struct _gqlLink;
struct _gqlLink;
struct _gqlType;
struct _gqlValue;

typedef struct _gqlQuery {
    struct _gqlQuery	*next;
    struct _gqlValue	*result;
    struct _gqlDirUse	*dir;
    // TBD data about a query, mutation, or subscription
} *gqlQuery;

typedef struct _gqlStrLink {
    struct _gqlStrLink	*next;
    char		*str;
} *gqlStrLink;

typedef struct _gqlTypeLink {
    struct _gqlTypeLink	*next;
    struct _gqlType	*type;
} *gqlTypeLink;

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
    bool		required;
} *gqlField;

typedef struct _gqlDir {
    struct _gqlDir	*next;
    const char		*name;
    const char		*desc;
    gqlArg		args;
    gqlStrLink		locs; // location names
    bool		defined;
    bool		core;
} *gqlDir;

typedef struct _gqlDirUse {
    struct _gqlDirUse	*next;
    gqlDir		dir;
    struct _gqlLink	*args;
} *gqlDirUse;

typedef struct _gqlType {
    const char		*name;
    const char		*desc;
    gqlDirUse		dir;
    gqlKind		kind;
    gqlScalarKind	scalar_kind;
    bool		core;
    union {
	struct { // Schema (just fields), Objects and Interfaces
	    gqlField		fields;
	    gqlTypeLink		interfaces;	// Types
	};
	gqlTypeLink		types;		// Union
	gqlEnumVal		choices;	// Enums
	gqlArg			args;		// InputObject
	struct {				// scalar
	    agooText		(*to_sdl)(agooText text, struct _gqlValue *value, int indent, int depth);
	    agooText		(*to_json)(agooText text, struct _gqlValue *value, int indent, int depth);
	    void		(*destroy)(struct _gqlValue *value);
	};
	struct { // List types
	    struct _gqlType	*base;
	    bool		not_empty;
	};
    };
} *gqlType;

// Execution Definition types
struct _gqlFrag;

typedef struct _gqlVar {
    struct _gqlVar	*next;
    const char		*name;
    gqlType		type;
    struct _gqlValue	*value;
} *gqlVar;

typedef struct _gqlSelArg {
    struct _gqlSelArg	*next;
    const char		*name;
    gqlVar		var;
    struct _gqlValue	*value;
} *gqlSelArg;

typedef struct _gqlSel {
    struct _gqlSel	*next;
    const char		*alias;
    const char		*name;
    gqlType		type; // set with validation
    gqlDirUse		dir;
    gqlSelArg		args;
    struct _gqlSel	*sels;
    const char		*frag;
    struct _gqlFrag	*inline_frag;
} *gqlSel;

typedef struct _gqlOp {
    struct _gqlOp	*next;
    const char		*name;
    gqlVar		vars;
    gqlDirUse		dir;
    gqlSel		sels;
    gqlOpKind		kind;
} *gqlOp;

typedef struct _gqlFrag {
    struct _gqlFrag	*next;
    const char		*name;
    gqlDirUse		dir;
    gqlType		on;
    gqlSel		sels;
} *gqlFrag;

typedef struct _gqlFuncs {
    gqlResolveFunc	resolve; // TBD change
    gqlTypeFunc		type;
} *gqlFuncs;

typedef struct _gqlDoc {
    gqlOp		ops;
    gqlVar		vars;
    gqlFrag		frags;
    gqlOp		op; // the op to execute
    struct _gqlFuncs	funcs;
} *gqlDoc;

extern int	gql_init(agooErr err);
extern void	gql_destroy(); // clear out all

extern gqlType	gql_schema_create(agooErr err, const char *desc, size_t dlen);
extern gqlType	gql_type_create(agooErr err, const char *name, const char *desc, size_t dlen, gqlTypeLink interfaces);
extern gqlType	gql_assure_type(agooErr err, const char *name);
extern int	gql_type_directive_use(agooErr err, gqlType type, gqlDirUse use);
extern bool	gql_type_has_directive_use(gqlType type, const char *dir);

extern gqlType	gql_input_create(agooErr err, const char *name, const char *desc, size_t dlen);
extern gqlType	gql_interface_create(agooErr err, const char *name, const char *desc, size_t dlen);

extern gqlField	gql_type_field(agooErr		err,
			       gqlType		type,
			       const char	*name,
			       gqlType		return_type,
			       struct _gqlValue	*default_value,
			       const char	*desc,
			       size_t		dlen,
			       bool 		required);

extern gqlArg	gql_field_arg(agooErr 		err,
			      gqlField 		field,
			      const char 	*name,
			      gqlType	 	type,
			      const char 	*desc,
			      size_t		dlen,
			      struct _gqlValue	*def_value,
			      bool 		required);

extern gqlArg	gql_input_arg(agooErr		err,
			      gqlType		input,
			      const char	*name,
			      gqlType		type,
			      const char	*desc,
			      size_t		dlen,
			      struct _gqlValue	*def_value,
			      bool		required);

extern gqlType		gql_scalar_create(agooErr err, const char *name, const char *desc, size_t dlen);

extern gqlDir		gql_directive_create(agooErr err, const char *name, const char *desc, size_t dlen);
extern gqlArg		gql_dir_arg(agooErr 		err,
				    gqlDir 		dir,
				    const char 		*name,
				    gqlType 		type,
				    const char	 	*desc,
				    size_t		dlen,
				    struct _gqlValue	*def_value,
				    bool 		required);
extern int		gql_directive_on(agooErr err, gqlDir d, const char *on, int len);
extern gqlDir		gql_directive_get(const char *name);

extern gqlDirUse	gql_dir_use_create(agooErr err, const char *name);
extern int		gql_dir_use_arg(agooErr err, gqlDirUse use, const char *key, struct _gqlValue *value);

extern gqlType		gql_union_create(agooErr err, const char *name, const char *desc, size_t dlen);
extern int		gql_union_add(agooErr err, gqlType type, gqlType member);

extern gqlType		gql_enum_create(agooErr err, const char *name, const char *desc, size_t dlen);
extern gqlEnumVal	gql_enum_append(agooErr err, gqlType type, const char *value, size_t len, const char *desc, size_t dlen);

extern gqlType		gql_assure_list(agooErr err, gqlType base, bool not_empty);

extern int		gql_type_set(agooErr err, gqlType type);
extern gqlType		gql_type_get(const char *name);
extern void		gql_type_destroy(gqlType type);
extern void		gql_type_iterate(void (*fun)(gqlType type, void *ctx), void *ctx);

extern agooText		gql_type_sdl(agooText text, gqlType type, bool comments);
extern agooText		gql_directive_sdl(agooText text, gqlDir dir, bool comments);
extern agooText		gql_schema_sdl(agooText text, bool with_desc, bool all);

extern agooText		gql_object_to_graphql(agooText text, struct _gqlValue *value, int indent, int depth);
extern agooText		gql_union_to_text(agooText text, struct _gqlValue *value, int indent, int depth);
extern agooText		gql_enum_to_text(agooText text, struct _gqlValue *value, int indent, int depth);

extern gqlDoc		gql_doc_create(agooErr err);
extern void		gql_doc_destroy(gqlDoc doc);

extern gqlOp		gql_op_create(agooErr err, const char *name, gqlOpKind kind);
extern gqlFrag		gql_fragment_create(agooErr err, const char *name, gqlType on);
extern agooText		gql_doc_sdl(gqlDoc doc, agooText text);

extern void		gql_dump_hook(struct _agooReq *req);
extern void		gql_eval_get_hook(struct _agooReq *req);
extern void		gql_eval_post_hook(struct _agooReq *req);

extern int		gql_validate(agooErr err);

extern gqlField		gql_type_get_field(gqlType type, const char *field);

extern gqlDir		gql_directives; // linked list

#endif // AGOO_GRAPHQL_H
