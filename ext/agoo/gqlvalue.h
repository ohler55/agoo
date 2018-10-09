// Copyright (c) 2018, Peter Ohler, All rights reserved.

#ifndef __AGOO_GQLVALUE_H__
#define __AGOO_GQLVALUE_H__

#include <stdbool.h>
#include <stdint.h>

#include "err.h"
#include "text.h"

struct _gqlType;

typedef struct _gqlLink {
    struct _gqlLink	*next;
    const char		*key;
    struct _gqlValue	*value;
} *gqlLink;

typedef struct _gqlValue {
    struct _gqlType	*type;
    union {
	const char	*url;
	int32_t		i;
	int64_t		i64;
	double		f;
	bool		b;
	int64_t		time;
	const char	*str;
	char		str16[16];
	struct {
	    uint64_t	hi;
	    uint64_t	lo;
	} uuid;
	struct _gqlLink	*members; // linked list for List and Object types
    };
} *gqlValue;

extern int	gql_value_init(Err err);

extern gqlValue	gql_value_create(Err err);
extern void	gql_value_destroy(gqlValue value);

extern gqlValue	gql_int_create(Err err, int32_t i);
extern gqlValue	gql_i64_create(Err err, int64_t i);
extern gqlValue	gql_string_create(Err err, const char *str);
extern gqlValue	gql_url_create(Err err, const char *url);
extern gqlValue	gql_bool_create(Err err, bool b);
extern gqlValue	gql_float_create(Err err, double f);
extern gqlValue	gql_time_create(Err err, int64_t t);
extern gqlValue	gql_time_str_create(Err err, const char *str);
extern gqlValue	gql_uuid_create(Err err, uint64_t hi, uint64_t lo);
extern gqlValue	gql_uuid_str_create(Err err, const char *str);
extern gqlValue	gql_null_create(Err err);
extern gqlValue	gql_list_create(Err err);
extern gqlValue	gql_object_create(Err err);

extern int	gql_list_append(Err err, gqlValue list, gqlValue item);
extern int	gql_object_append(Err err, gqlValue list, const char *key, gqlValue item);

extern void	gql_int_set(gqlValue value, int32_t i);
extern void	gql_i64_set(gqlValue value, int64_t i);
extern void	gql_string_set(gqlValue value, const char *str);
extern int	gql_url_set(Err err, gqlValue value, const char *url);
extern void	gql_bool_set(gqlValue value, bool b);
extern void	gql_float_set(gqlValue value, double f);
extern void	gql_time_set(gqlValue value, int64_t t);
extern int	gql_time_str_set(Err err, gqlValue value, const char *str);
extern void	gql_uuid_set(gqlValue value, uint64_t hi, uint64_t lo);
extern int	gql_uuid_str_set(Err err, gqlValue value, const char *str);
extern void	gql_null_set(gqlValue value);

extern Text	gql_value_text(Text text, gqlValue value, int indent);

#endif // __AGOO_GQLVALUE_H__
