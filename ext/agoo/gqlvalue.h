// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef AGOO_GQLVALUE_H
#define AGOO_GQLVALUE_H

#include <stdbool.h>
#include <stdint.h>

#include "err.h"
#include "text.h"

struct _gqlType;

typedef struct _gqlLink {
    struct _gqlLink	*next;
    char		*key;
    struct _gqlValue	*value;
} *gqlLink;

typedef struct _gqlValue {
    struct _gqlType		*type;
    union {
	int32_t			i;
	int64_t			i64;
	double			f;
	bool			b;
	int64_t			time;
	union {
	    struct {
		char		alloced;
		char		a[15];
	    };
	    struct {
		char		x; // same memory location as kind
		char		pad[7];
		const char	*ptr;
	    };
	} str;
	struct {
	    uint64_t		hi;
	    uint64_t		lo;
	} uuid;
	struct {
	    struct _gqlLink	*members; // linked list for List and Object types
	    struct _gqlType	*member_type;
	};
    };
} *gqlValue;

extern int	gql_value_init(agooErr err);

extern void	gql_value_destroy(gqlValue value);
extern gqlValue	gql_value_dup(agooErr err, gqlValue value);

extern gqlLink	gql_link_create(agooErr err, const char *key, gqlValue value);
extern void	gql_link_destroy(gqlLink link);

extern gqlValue	gql_int_create(agooErr err, int32_t i);
extern gqlValue	gql_i64_create(agooErr err, int64_t i);
extern gqlValue	gql_string_create(agooErr err, const char *str, int len);
extern gqlValue	gql_id_create(agooErr err, const char *str, int len);
extern gqlValue	gql_token_create(agooErr err, const char *str, int len, struct _gqlType *type);
extern gqlValue	gql_var_create(agooErr err, const char *str, int len);
extern gqlValue	gql_bool_create(agooErr err, bool b);
extern gqlValue	gql_float_create(agooErr err, double f);
extern gqlValue	gql_time_create(agooErr err, int64_t t);
extern gqlValue	gql_time_str_create(agooErr err, const char *str, int len);
extern gqlValue	gql_uuid_create(agooErr err, uint64_t hi, uint64_t lo);
extern gqlValue	gql_uuid_str_create(agooErr err, const char *str, int len);
extern gqlValue	gql_null_create(agooErr err);
extern gqlValue	gql_list_create(agooErr err, struct _gqlType *item_type);
extern gqlValue	gql_object_create(agooErr err);

extern int	gql_list_append(agooErr err, gqlValue list, gqlValue item);
extern int	gql_list_preend(agooErr err, gqlValue list, gqlValue item);
extern int	gql_object_set(agooErr err, gqlValue obj, const char *key, gqlValue item);

extern void	gql_int_set(gqlValue value, int32_t i);
extern void	gql_i64_set(gqlValue value, int64_t i);
extern int	gql_string_set(agooErr err, gqlValue value, const char *str, int len);
extern int	gql_id_set(agooErr err, gqlValue value, const char *str, int len);
extern int	gql_token_set(agooErr err, gqlValue value, const char *str, int len);
extern void	gql_bool_set(gqlValue value, bool b);
extern void	gql_float_set(gqlValue value, double f);
extern void	gql_time_set(gqlValue value, int64_t t);
extern int	gql_time_str_set(agooErr err, gqlValue value, const char *str, int len);
extern void	gql_uuid_set(gqlValue value, uint64_t hi, uint64_t lo);
extern int	gql_uuid_str_set(agooErr err, gqlValue value, const char *str, int len);
extern void	gql_null_set(gqlValue value);

extern const char*	gql_string_get(gqlValue value);

extern agooText	gql_value_json(agooText text, gqlValue value, int indent, int depth);
extern agooText	gql_value_sdl(agooText text, gqlValue value, int indent, int depth);

//extern agooText	gql_object_to_json(agooText text, gqlValue value, int indent, int depth);
extern agooText	gql_object_to_sdl(agooText text, gqlValue value, int indent, int depth);

extern int	gql_value_convert(agooErr err, gqlValue value, struct _gqlType *type);

extern struct _gqlType	gql_null_type;
extern struct _gqlType	gql_int_type;
extern struct _gqlType	gql_i64_type;
extern struct _gqlType	gql_bool_type;
extern struct _gqlType	gql_float_type;
extern struct _gqlType	gql_time_type;
extern struct _gqlType	gql_uuid_type;
extern struct _gqlType	gql_string_type;
extern struct _gqlType	gql_token_type;   // used for enum values
extern struct _gqlType	gql_id_type;
extern struct _gqlType	gql_var_type;     // used for variable keys

#endif // AGOO_GQLVALUE_H
