// Copyright 2016, 2018 by Peter Ohler, All Rights Reserved

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "debug.h"
#include "dtime.h"
#include "page.h"

#define PAGE_RECHECK_TIME	5.0

#define MAX_KEY_UNIQ		9
#define MAX_KEY_LEN		1024
#define PAGE_BUCKET_SIZE	1024
#define PAGE_BUCKET_MASK	1023

#define MAX_MIME_KEY_LEN	15
#define MIME_BUCKET_SIZE	64
#define MIME_BUCKET_MASK	63

typedef struct _slot {
    struct _slot	*next;
    char		key[MAX_KEY_LEN + 1];
    agooPage		value;
    uint64_t		hash;
    int			klen;
} *Slot;

typedef struct _headRule {
    struct _headRule	*next;
    char		*path;
    char		*mime;
    char		*key;
    char		*value;
    int			len; // length of key + value + ': ' + '\r\n'
} *HeadRule;

typedef struct _mimeSlot {
    struct _mimeSlot	*next;
    char		key[MAX_MIME_KEY_LEN + 1];
    char		*value;
    uint64_t		hash;
    int			klen;
} *MimeSlot;

typedef struct _cache {
    Slot		buckets[PAGE_BUCKET_SIZE];
    Slot		ruckets[PAGE_BUCKET_SIZE];
    MimeSlot		muckets[MIME_BUCKET_SIZE];
    char		*root;
    agooGroup		groups;
    HeadRule		head_rules;
} *Cache;

typedef struct _mime {
    const char	*suffix;
    const char	*type;
} *Mime;

// These are used for the initial load.
static struct _mime	mime_map[] = {
    { "asc", "text/plain" },
    { "avi", "video/x-msvideo" },
    { "bin", "application/octet-stream" },
    { "bmp", "image/bmp" },
    { "cer", "application/pkix-cert" },
    { "crl", "application/pkix-crl" },
    { "crt", "application/x-x509-ca-cert" },
    { "css", "text/css" },
    { "doc", "application/msword" },
    { "eot", "application/vnd.ms-fontobject" },
    { "eps", "application/postscript" },
    { "es5", "application/javascript" },
    { "es6", "application/javascript" },
    { "gif", "image/gif" },
    { "htm", "text/html" },
    { "html", "text/html" },
    { "ico", "image/x-icon" },
    { "jpeg", "image/jpeg" },
    { "jpg", "image/jpeg" },
    { "js", "application/javascript" },
    { "json", "application/json" },
    { "mov", "video/quicktime" },
    { "mpe", "video/mpeg" },
    { "mpeg", "video/mpeg" },
    { "mpg", "video/mpeg" },
    { "pdf", "application/pdf" },
    { "png", "image/png" },
    { "ppt", "application/vnd.ms-powerpoint" },
    { "ps", "application/postscript" },
    { "qt", "video/quicktime" },
    { "rb", "text/plain" },
    { "rtf", "application/rtf" },
    { "sse", "text/plain" },
    { "svg", "image/svg+xml" },
    { "tif", "image/tiff" },
    { "tiff", "image/tiff" },
    { "ttf", "application/font-sfnt" },
    { "txt", "text/plain" },
    { "woff", "application/font-woff" },
    { "woff2", "font/woff2" },
    { "xls", "application/vnd.ms-excel" },
    { "xml", "application/xml" },
    { "zip", "application/zip" },
    { NULL, NULL }
};

static const char	page_fmt[] = "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n";
static const char	page_min_fmt[] = "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n";

static struct _cache	cache = {
    .buckets = {0},
    .ruckets = {0},
    .muckets = {0},
    .root = NULL,
    .groups = NULL,
    .head_rules = NULL,
};

static uint64_t
calc_hash(const char *key, int *lenp) {
    int			len = 0;
    int			klen = *lenp;
    uint64_t		h = 0;
    bool		special = false;
    const uint8_t	*k = (const uint8_t*)key;

    for (; len < klen; k++) {
	// narrow to most used range of 0x4D (77) in size
	if (*k < 0x2D || 0x7A < *k) {
	    special = true;
	}
	// fast, just spread it out
	h = 77 * h + (*k - 0x2D);
	len++;
    }

    if (special) {
	*lenp = -len;
    } else {
	*lenp = len;
    }
    return h;
}

// Buckets are a twist on the hash to mix it up a bit. Odd shifts and XORs.
static Slot*
get_bucketp(uint64_t h) {
    return cache.buckets + (PAGE_BUCKET_MASK & (h ^ (h << 5) ^ (h >> 7)));
}

static Slot*
get_rucketp(uint64_t h) {
    return cache.ruckets + (PAGE_BUCKET_MASK & (h ^ (h << 5) ^ (h >> 7)));
}

static MimeSlot*
get_mime_bucketp(uint64_t h) {
    return cache.muckets + (MIME_BUCKET_MASK & (h ^ (h << 5) ^ (h >> 7)));
}

const char*
mime_get(const char *key) {
    int		klen = (int)strlen(key);
    int		len = klen;
    int64_t	h = calc_hash(key, &len);
    MimeSlot	*bucket = get_mime_bucketp(h);
    MimeSlot	s;
    const char	*v = NULL;

    for (s = *bucket; NULL != s; s = s->next) {
	if (h == (int64_t)s->hash && len == (int)s->klen &&
	    ((0 <= len && len <= MAX_KEY_UNIQ) || 0 == strncmp(s->key, key, klen))) {
	    v = s->value;
	    break;
	}
    }
    return v;
}

agooPage
cache_get(const char *key, int klen) {
    int		len = klen;
    int64_t	h = calc_hash(key, &len);
    Slot	*bucket = get_bucketp(h);
    Slot	s;
    agooPage	v = NULL;

    for (s = *bucket; NULL != s; s = s->next) {
	if (h == (int64_t)s->hash && len == (int)s->klen &&
	    ((0 <= len && len <= MAX_KEY_UNIQ) || 0 == strncmp(s->key, key, klen))) {
	    v = s->value;
	    break;
	}
    }
    return v;
}

agooPage
cache_root_get(const char *key, int klen) {
    int		len = klen;
    int64_t	h = calc_hash(key, &len);
    Slot	*bucket = get_rucketp(h);
    Slot	s;
    agooPage	v = NULL;

    for (s = *bucket; NULL != s; s = s->next) {
	if (h == (int64_t)s->hash && len == (int)s->klen &&
	    ((0 <= len && len <= MAX_KEY_UNIQ) || 0 == strncmp(s->key, key, klen))) {
	    v = s->value;
	    break;
	}
    }
    return v;
}

int
mime_set(agooErr err, const char *key, const char *value) {
    int		klen = (int)strlen(key);
    int		len = klen;
    int64_t	h = calc_hash(key, &len);
    MimeSlot	*bucket = get_mime_bucketp(h);
    MimeSlot	s;

    if (MAX_MIME_KEY_LEN < len) {
	return agoo_err_set(err, AGOO_ERR_ARG, "%s is too long for a file extension. Maximum is %d", key, MAX_MIME_KEY_LEN);
    }
    for (s = *bucket; NULL != s; s = s->next) {
	if (h == (int64_t)s->hash && len == s->klen &&
	    ((0 <= len && len <= MAX_KEY_UNIQ) || 0 == strncmp(s->key, key, len))) {

	    AGOO_FREE(s->value);
	    if (NULL == (s->value = AGOO_STRDUP(value))) {
		return agoo_err_set(err, AGOO_ERR_ARG, "out of memory adding %s", key);
	    }
	    return AGOO_ERR_OK;
	}
    }
    if (NULL == (s = (MimeSlot)AGOO_MALLOC(sizeof(struct _mimeSlot)))) {
	return AGOO_ERR_MEM(err, "mime key and value");
    }
    s->hash = h;
    s->klen = len;
    if (NULL == key) {
	*s->key = '\0';
    } else {
	strcpy(s->key, key);
    }
    if (NULL == (s->value = AGOO_STRDUP(value))) {
	AGOO_FREE(s);
	return AGOO_ERR_MEM(err, "mime key and value");
    }
    s->next = *bucket;
    *bucket = s;

    return AGOO_ERR_OK;
}

static agooPage
bucket_set(Slot *bucket, int64_t h, const char *key, int klen, agooPage value) {
    Slot	s;
    agooPage	old = NULL;

    if (MAX_KEY_LEN < klen) {
	return value;
    }
    for (s = *bucket; NULL != s; s = s->next) {
	if (h == (int64_t)s->hash && klen == s->klen &&
	    ((0 <= klen && klen <= MAX_KEY_UNIQ) || 0 == strncmp(s->key, key, klen))) {

	    old = s->value;
	    // replace
	    s->value = value;

	    return old;
	}
    }
    if (NULL == (s = (Slot)AGOO_MALLOC(sizeof(struct _slot)))) {
	return value;
    }
    s->hash = h;
    s->klen = klen;
    if (NULL == key) {
	*s->key = '\0';
    } else {
	strncpy(s->key, key, klen);
	s->key[klen] = '\0';
    }
    s->value = value;
    s->next = *bucket;
    *bucket = s;

    return old;
}

static agooPage
cache_set(const char *key, int klen, agooPage value) {
    int		len = klen;
    int64_t	h = calc_hash(key, &len);
    Slot	*bucket = get_bucketp(h);

    return bucket_set(bucket, h, key, len, value);
}

static agooPage
cache_root_set(const char *key, int klen, agooPage value) {
    int		len = klen;
    int64_t	h = calc_hash(key, &len);
    Slot	*bucket = get_rucketp(h);

    return bucket_set(bucket, h, key, len, value);
}

int
agoo_pages_init(agooErr err) {
    Mime	m;

    memset(&cache, 0, sizeof(struct _cache));
    if (NULL == (cache.root = AGOO_STRDUP("."))) {
	return agoo_err_set(err, AGOO_ERR_ARG, "out of memory allocating root path");
    }
    for (m = mime_map; NULL != m->suffix; m++) {
	mime_set(err, m->suffix, m->type);
    }
    return err->code;
}

int
agoo_pages_set_root(agooErr err, const char *root) {
    AGOO_FREE(cache.root);
    if (NULL == root) {
	cache.root = NULL;
    } else {
	if (NULL == (cache.root = AGOO_STRDUP(root))) {
	    return agoo_err_set(err, AGOO_ERR_ARG, "out of memory allocating root path");
	}
    }
    return AGOO_ERR_OK;
}

static void
agoo_page_destroy(agooPage p) {
    if (NULL != p->resp) {
	agoo_text_release(p->resp);
	p->resp = NULL;
    }
    AGOO_FREE(p->path);
    AGOO_FREE(p);
}

void
agoo_pages_cleanup() {
    Slot	*sp = cache.buckets;
    Slot	s;
    Slot	n;
    MimeSlot	*mp = cache.muckets;
    MimeSlot	sm;
    MimeSlot	m;
    HeadRule	hr;
    int		i;

    for (i = PAGE_BUCKET_SIZE; 0 < i; i--, sp++) {
	for (s = *sp; NULL != s; s = n) {
	    n = s->next;
	    agoo_page_destroy(s->value);
	    AGOO_FREE(s);
	}
	*sp = NULL;
    }
    for (i = MIME_BUCKET_SIZE; 0 < i; i--, mp++) {
	for (sm = *mp; NULL != sm; sm = m) {
	    m = sm->next;
	    AGOO_FREE(sm->value);
	    AGOO_FREE(sm);
	}
	*mp = NULL;
    }
    while (NULL != (hr = cache.head_rules)) {
	cache.head_rules = hr->next;
	AGOO_FREE(hr->path);
	AGOO_FREE(hr->mime);
	AGOO_FREE(hr->key);
	AGOO_FREE(hr->value);
	AGOO_FREE(hr);
    }
    AGOO_FREE(cache.root);
}

static const char*
path_mime(const char *path) {
    const char	*suffix = path + strlen(path) - 1;
    const char	*mime = NULL;

    for (; '.' != *suffix; suffix--) {
	if (suffix <= path) {
	    suffix = NULL;
	    break;
	}
    }
    if (suffix <= path) {
	suffix = NULL;
    }
    if (NULL != suffix) {
	suffix++;
	mime = mime_get(suffix);
    }
    return mime;
}

static const char*
path_suffix(const char *path) {
    const char	*suffix = path + strlen(path) - 1;

    for (; '.' != *suffix; suffix--) {
	if (suffix <= path) {
	    suffix = NULL;
	    break;
	}
    }
    if (suffix <= path) {
	suffix = NULL;
    }
    if (NULL != suffix) {
	suffix++;
    }
    return suffix;
}

static bool
path_match(const char *pat, const char *path) {
    if (NULL == pat) {
	return true;
    }
    if (NULL == path) {
	return '*' == *pat && '*' == pat[1] && '\0' == pat[2];
    }
    if ('/' == *path) {
	path++;
    }
    for (; '\0' != *pat && '\0' != *path; pat++) {
	if (*path == *pat) {
	    path++;
	} else if ('*' == *pat) {
	    if ('*' == *(pat + 1)) {
		return true;
	    }
	    for (; '\0' != *path && '/' != *path; path++) {
	    }
	} else {
	    break;
	}
    }
    return '\0' == *pat && '\0' == *path;
}

static bool
head_rule_match(HeadRule rule, const char *path, const char *mime) {
    const char	*suffix = NULL;

    if (NULL != path) {
	suffix = path_suffix(path);
    }
    return path_match(rule->path, path) &&
	(NULL == rule->mime || 0 == strcmp(rule->mime, mime) ||
	 (NULL != suffix && 0 == strcmp(rule->mime, suffix)));
}

// The page resp points to the page resp msg to save memory and reduce
// allocations.
agooPage
agoo_page_create(const char *path) {
    agooPage	p = (agooPage)AGOO_MALLOC(sizeof(struct _agooPage));

    if (NULL != p) {
	p->resp = NULL;
	if (NULL == path) {
	    p->path = NULL;
	} else {
	    if (NULL == (p->path = AGOO_STRDUP(path))) {
		AGOO_FREE(p);
		return NULL;
	    }
	}
	p->mtime = 0;
	p->last_check = 0.0;
	p->immutable = false;
    }
    return p;
}

agooPage
agoo_page_immutable(agooErr err, const char *path, const char *content, int clen) {
    agooPage	p = (agooPage)AGOO_MALLOC(sizeof(struct _agooPage));
    const char	*mime = path_mime(path);
    char	*rel_path = NULL;
    long	msize;
    int		cnt;
    int		plen = 0;
    long	hlen = 0;
    HeadRule	hr;

    if (NULL == p) {
	AGOO_ERR_MEM(err, "Page");
	return NULL;
    }
    if (NULL == path) {
	p->path = NULL;
    } else {
	if (NULL == (p->path = AGOO_STRDUP(path))) {
	    AGOO_FREE(p);
	    return NULL;
	}
	plen = (int)strlen(path);
	if (NULL != cache.root) {
	    int	rlen = strlen(cache.root);

	    if (0 == strncmp(cache.root, p->path, rlen) && '/' == p->path[rlen]) {
		rel_path = p->path + rlen + 1;
	    } else {
		rel_path = NULL;
	    }
	}
    }
    p->mtime = 0;
    p->last_check = 0.0;
    p->immutable = true;

    if (NULL == mime) {
	mime = "text/html";
    }
    if (0 == clen) {
	clen = (int)strlen(content);
    }
    for (hr = cache.head_rules; NULL != hr; hr = hr->next) {
	if (head_rule_match(hr, rel_path, mime)) {
	    hlen += hr->len;
	}
    }
    // Format size plus space for the length, the mime type, and some
    // padding. Then add the content length.
    msize = sizeof(page_fmt) + 60 + clen;
    if (NULL == (p->resp = agoo_text_allocate((int)msize))) {
	AGOO_ERR_MEM(err, "Page content");
	AGOO_FREE(p);
	return NULL;
    }
    if (0 < hlen) {
	bool	has_ct;

	cnt = sprintf(p->resp->text, page_min_fmt, (long)clen); // HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n
	for (hr = cache.head_rules; NULL != hr; hr = hr->next) {
	    if (head_rule_match(hr, rel_path, mime)) {
		cnt += sprintf(p->resp->text + cnt, "%s: %s\r\n", hr->key, hr->value);
		if (0 == strcasecmp("Content-Type", hr->key)) {
		    has_ct = true;
		}
	    }
	}
	if (!has_ct) {
	    cnt += sprintf(p->resp->text + cnt, "Content-Type: %s\r\n\r\n", mime);
	} else {
	    strcpy(p->resp->text + cnt, "\r\n");
	    cnt += 2;
	}
    } else {
	cnt = sprintf(p->resp->text, page_fmt, mime, (long)clen);
    }
    msize = cnt + clen;
    memcpy(p->resp->text + cnt, content, clen);
    p->resp->text[msize] = '\0';
    p->resp->len = msize;
    agoo_text_ref(p->resp);

    cache_set(path, plen, p);

    return p;
}

static bool
close_return_false(FILE *f) {
    fclose(f);
    return false;
}

static bool
update_contents(agooPage p) {
    const char	*mime = path_mime(p->path);
    char	path[1024];
    char	*rel_path = NULL;
    int		plen = (int)strlen(p->path);
    long	size;
    struct stat	fattr;
    long	msize;
    long	hlen = 0;
    int		cnt;
    struct stat	fs;
    agooText	t;
    FILE	*f = fopen(p->path, "rb");
    HeadRule	hr;

    strncpy(path, p->path, sizeof(path));
    path[sizeof(path) - 1] = '\0';
    // On linux a directory is opened by fopen (sometimes? all the time?) so
    // fstat is called to get the file mode and verify it is a regular file or
    // a symlink.
    if (NULL != f) {
	if (0 == fstat(fileno(f), &fs)) {
	    if (!S_ISREG(fs.st_mode) && !S_ISLNK(fs.st_mode)) {
		fclose(f);
		f = NULL;
	    }
	} else {
	    return close_return_false(f);
	}
    }
    if (NULL == f) {
	// If not found how about with a /index.html added?
	if (NULL == mime) {
	    int		cnt;

	    if ('/' == p->path[plen - 1]) {
		cnt = snprintf(path, sizeof(path), "%sindex.html", p->path);
	    } else {
		cnt = snprintf(path, sizeof(path), "%s/index.html", p->path);
	    }
	    if ((int)sizeof(path) < cnt) {
		return false;
	    }
	    if (NULL == (f = fopen(path, "rb"))) {
		return false;
	    }
	    mime = "text/html";
	} else {
	    return false;
	}
    }
    if (NULL == mime) {
	mime = "text/html";
    }
    if (0 != fseek(f, 0, SEEK_END)) {
	return close_return_false(f);
    }
    if (0 > (size = ftell(f))) {
	return close_return_false(f);
    }
    rewind(f);

    if (NULL != cache.root) {
	int	rlen = strlen(cache.root);

	if (0 == strncmp(cache.root, path, rlen) && '/' == path[rlen]) {
	    rel_path = path + rlen + 1;
	}
    }
    for (hr = cache.head_rules; NULL != hr; hr = hr->next) {
	if (head_rule_match(hr, rel_path, mime)) {
	    hlen += hr->len;
	}
    }
    // Format size plus space for the length, the mime type, and some
    // padding. Then add the header rule and content length.
    msize = sizeof(page_fmt) + 60 + size + hlen;
    if (NULL == (t = agoo_text_allocate((int)msize))) {
	return close_return_false(f);
    }
    if (0 < hlen) {
	bool	has_ct = false;

	cnt = sprintf(t->text, page_min_fmt, size); // HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n
	for (hr = cache.head_rules; NULL != hr; hr = hr->next) {
	    if (head_rule_match(hr, rel_path, mime)) {
		cnt += sprintf(t->text + cnt, "%s: %s\r\n", hr->key, hr->value);
		if (0 == strcasecmp("Content-Type", hr->key)) {
		    has_ct = true;
		}
	    }
	}
	if (!has_ct) {
	    cnt += sprintf(t->text + cnt, "Content-Type: %s\r\n\r\n", mime);
	} else {
	    strcpy(t->text + cnt, "\r\n");
	    cnt += 2;
	}
    } else {
	cnt = sprintf(t->text, page_fmt, mime, size);
    }
    msize = cnt + size;
    if (0 < size) {
	if (size != (long)fread(t->text + cnt, 1, size, f)) {
	    agoo_text_release(t);
	    return close_return_false(f);
	}
    }
    fclose(f);
    t->text[msize] = '\0';
    t->len = msize;
    if (0 == stat(p->path, &fattr)) {
	p->mtime = fattr.st_mtime;
    } else {
	p->mtime = 0;
    }
    if (NULL != p->resp) {
	agoo_text_release(p->resp);
	p->resp = NULL;
    }
    p->resp = t;
    agoo_text_ref(p->resp);
    p->last_check = dtime();

    return true;
}

static void
agoo_page_remove(agooPage p) {
    int		len = (int)strlen(p->path);
    int64_t	h = calc_hash(p->path, &len);
    Slot	*bucket = get_bucketp(h);
    Slot	s;
    Slot	prev = NULL;

    for (s = *bucket; NULL != s; s = s->next) {
	if (h == (int64_t)s->hash && len == (int)s->klen &&
	    ((0 <= len && len <= MAX_KEY_UNIQ) || 0 == strncmp(s->key, p->path, len))) {

	    if (NULL == prev) {
		*bucket = s->next;
	    } else {
		prev->next = s->next;
	    }
	    agoo_page_destroy(s->value);
	    AGOO_FREE(s);

	    break;
	}
	prev = s;
    }
}

static agooPage
page_check(agooErr err, agooPage page) {
    if (!page->immutable) {
	double	now = dtime();

	if (page->last_check + PAGE_RECHECK_TIME < now) {
	    struct stat	fattr;

	    if (0 == stat(page->path, &fattr) && page->mtime != fattr.st_mtime) {
		update_contents(page);
		if (NULL == page->resp) {
		    agoo_page_remove(page);
		    agoo_err_set(err, AGOO_ERR_NOT_FOUND, "not found.");
		    return NULL;
		}
	    }
	    page->last_check = now;
	}
    }
    return page;
}

agooPage
agoo_page_get(agooErr err, const char *path, int plen, const char *root) {
    agooPage	page = NULL;

    if (NULL != strstr(path, "../")) {
	return NULL;
    }
    if (NULL != root) {
	char	full_path[2048];
	char	*s = stpcpy(full_path, root);

	if (NULL != strstr(path, "../")) {
	    return NULL;
	}
	if ((int)sizeof(full_path) <= plen + (s - full_path)) {
	    AGOO_ERR_MEM(err, "Page path");
	    return NULL;
	}
	if ('/' != *root && '/' != *(s - 1) && '/' != *path) {
	    *s++ = '/';
	}
	strncpy(s, path, plen);
	s[plen] = '\0';

	if (NULL == (page = cache_root_get(full_path, plen))) {
	    if (NULL != cache.root) {
		agooPage	old;

		if (NULL == (page = agoo_page_create(full_path))) {
		    AGOO_ERR_MEM(err, "Page");
		    return NULL;
		}
		if (!update_contents(page) || NULL == page->resp) {
		    agoo_page_destroy(page);
		    agoo_err_set(err, AGOO_ERR_NOT_FOUND, "not found.");
		    return NULL;
		}
		if (NULL != (old = cache_root_set(full_path, plen, page))) {
		    agoo_page_destroy(old);
		}
	    }
	} else {
	    page = page_check(err, page);
	}
    } else {
	if (NULL == (page = cache_get(path, plen))) {
	    if (NULL != cache.root) {
		agooPage	old;
		char	full_path[2048];
		char	*s = stpcpy(full_path, cache.root);

		if ('/' != *cache.root && '/' != *path) {
		    *s++ = '/';
		}
		if ((int)sizeof(full_path) <= plen + (s - full_path)) {
		    AGOO_ERR_MEM(err, "Page path");
		    return NULL;
		}
		strncpy(s, path, plen);
		s[plen] = '\0';
		if (NULL == (page = agoo_page_create(full_path))) {
		    AGOO_ERR_MEM(err, "Page");
		    return NULL;
		}
		if (!update_contents(page) || NULL == page->resp) {
		    agoo_page_destroy(page);
		    agoo_err_set(err, AGOO_ERR_NOT_FOUND, "not found.");
		    return NULL;
		}
		if (NULL != (old = cache_set(path, plen, page))) {
		    agoo_page_destroy(old);
		}
	    }
	} else {
	    page = page_check(err, page);
	}
    }
    return page;
}

agooPage
agoo_group_get(agooErr err, const char *path, int plen) {
    agooPage	page = NULL;
    agooGroup	g = NULL;
    char	full_path[2048];
    char	*s = NULL;
    agooDir	d;

    if (NULL != strstr(path, "../")) {
	return NULL;
    }
    for (g = cache.groups; NULL != g; g = g->next) {
	if (g->plen < plen && 0 == strncmp(path, g->path, g->plen) && '/' == path[g->plen]) {
	    break;
	}
    }
    if (NULL == g) {
	return NULL;
    }
    for (d = g->dirs; NULL != d; d = d->next) {
	if ((int)sizeof(full_path) <= d->plen + plen) {
	    continue;
	}
	s = stpcpy(full_path, d->path);
	strncpy(s, path + g->plen, plen - g->plen);
	s += plen - g->plen;
	*s = '\0';
	if (NULL != (page = cache_get(full_path, (int)(s - full_path)))) {
	    break;
	}
    }
    if (NULL == page) {
	for (d = g->dirs; NULL != d; d = d->next) {
	    if ((int)sizeof(full_path) <= d->plen + plen) {
		continue;
	    }
	    s = stpcpy(full_path, d->path);
	    strncpy(s, path + g->plen, plen - g->plen);
	    s += plen - g->plen;
	    *s = '\0';
	    if (0 == access(full_path, R_OK)) {
		break;
	    }
	}
	if (NULL == d) {
	    return NULL;
	}
	plen = (int)(s - full_path);
	path = full_path;
	if (NULL == (page = cache_get(path, plen))) {
	    agooPage	old;

	    if (NULL == (page = agoo_page_create(path))) {
		AGOO_ERR_MEM(err, "Page");
		return NULL;
	    }
	    if (!update_contents(page) || NULL == page->resp) {
		agoo_page_destroy(page);
		agoo_err_set(err, AGOO_ERR_NOT_FOUND, "not found.");
		return NULL;
	    }
	    if (NULL != (old = cache_set(path, plen, page))) {
		agoo_page_destroy(old);
	    }
	}
	return page;
    }
    return page_check(err, page);
}

agooGroup
agoo_group_create(const char *path) {
    agooGroup	g = (agooGroup)AGOO_MALLOC(sizeof(struct _agooGroup));

    if (NULL != g) {
	if (NULL == (g->path = AGOO_STRDUP(path))) {
	    AGOO_FREE(g);
	    return NULL;
	}
	g->plen = (int)strlen(path);
	g->dirs = NULL;
	g->next = cache.groups;
	cache.groups = g;
    }
    return g;
}

agooDir
agoo_group_add(agooErr err, agooGroup g, const char *dir) {
    agooDir	d = (agooDir)AGOO_MALLOC(sizeof(struct _agooDir));

    if (NULL != d) {
	if (NULL == (d->path = AGOO_STRDUP(dir))) {
	    AGOO_ERR_MEM(err, "Group");
	    return NULL;
	}
	d->plen = (int)strlen(dir);
	d->next = g->dirs;
	g->dirs = d;
    }
    return d;
}

int
agoo_header_rule(agooErr err, const char *path, const char *mime, const char *key, const char *value) {
    HeadRule	hr;

    if (0 == strcasecmp("Content-Length", key)) {
	return agoo_err_set(err, AGOO_ERR_ARG, "Can not mask Content-Length with a header rule.");
    }
    if (NULL == (hr = (HeadRule)AGOO_CALLOC(1, sizeof(struct _headRule)))) {
	return AGOO_ERR_MEM(err, "Header Rule");
    }
    if (NULL == (hr->path = AGOO_STRDUP(path)) ||
	NULL == (hr->key = AGOO_STRDUP(key)) ||
	NULL == (hr->value = AGOO_STRDUP(value))) {
	goto ERROR;
    }
    if ('*' == *mime && '\0' == mime[1]) {
	hr->mime = NULL;
    } else if (NULL == (hr->mime = AGOO_STRDUP(mime))) {
	goto ERROR;
    }
    hr->len = strlen(hr->key) + strlen(hr->value) + 4;
    hr->next = cache.head_rules;
    cache.head_rules = hr;

    return AGOO_ERR_OK;

ERROR:
    AGOO_FREE(hr->path);
    AGOO_FREE(hr->key);
    AGOO_FREE(hr->value);
    AGOO_FREE(hr);

    return AGOO_ERR_MEM(err, "Header Rule");
}
