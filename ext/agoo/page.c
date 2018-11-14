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

typedef struct _Slot {
    struct _Slot	*next;
    char		key[MAX_KEY_LEN + 1];
    Page		value;
    uint64_t		hash;
    int			klen;
} *Slot;

typedef struct _MimeSlot {
    struct _MimeSlot	*next;
    char		key[MAX_MIME_KEY_LEN + 1];
    char		*value;
    uint64_t		hash;
    int			klen;
} *MimeSlot;

typedef struct _Cache {
    Slot		buckets[PAGE_BUCKET_SIZE];
    MimeSlot		muckets[MIME_BUCKET_SIZE];
    char		*root;
    Group		groups;
} *Cache;

typedef struct _Mime {
    const char	*suffix;
    const char	*type;
} *Mime;

// These are used for the initial load.
static struct _Mime	mime_map[] = {
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

static struct _Cache	cache = {
    .buckets = {0},
    .muckets = {0},
    .root = NULL,
    .groups = NULL,
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

Page
cache_get(const char *key, int klen) {
    int		len = klen;
    int64_t	h = calc_hash(key, &len);
    Slot	*bucket = get_bucketp(h);
    Slot	s;
    Page	v = NULL;

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
mime_set(Err err, const char *key, const char *value) {
    int		klen = (int)strlen(key);
    int		len = klen;
    int64_t	h = calc_hash(key, &len);
    MimeSlot	*bucket = get_mime_bucketp(h);
    MimeSlot	s;
    
    if (MAX_MIME_KEY_LEN < len) {
	return err_set(err, ERR_ARG, "%s is too long for a file extension. Maximum is %d", key, MAX_MIME_KEY_LEN);
    }
    for (s = *bucket; NULL != s; s = s->next) {
	if (h == (int64_t)s->hash && len == s->klen &&
	    ((0 <= len && len <= MAX_KEY_UNIQ) || 0 == strncmp(s->key, key, len))) {

	    DEBUG_FREE(mem_mime_slot, s->value);
	    free(s->value);
	    s->value = strdup(value);
	    return ERR_OK;
	}
    }
    if (NULL == (s = (MimeSlot)malloc(sizeof(struct _MimeSlot)))) {
	return err_set(err, ERR_ARG, "out of memory adding %s", key);
    }
    DEBUG_ALLOC(mem_mime_slot, s);
    s->hash = h;
    s->klen = len;
    if (NULL == key) {
	*s->key = '\0';
    } else {
	strcpy(s->key, key);
    }
    s->value = strdup(value);
    s->next = *bucket;
    *bucket = s;

    return ERR_OK;
}

static Page
cache_set(const char *key, int klen, Page value) {
    int		len = klen;
    int64_t	h = calc_hash(key, &len);
    Slot	*bucket = get_bucketp(h);
    Slot	s;
    Page	old = NULL;
    
    if (MAX_KEY_LEN < len) {
	return value;
    }
    for (s = *bucket; NULL != s; s = s->next) {
	if (h == (int64_t)s->hash && len == s->klen &&
	    ((0 <= len && len <= MAX_KEY_UNIQ) || 0 == strncmp(s->key, key, len))) {

	    old = s->value;
	    // replace
	    s->value = value;

	    return old;
	}
    }
    if (NULL == (s = (Slot)malloc(sizeof(struct _Slot)))) {
	return value;
    }
    DEBUG_ALLOC(mem_page_slot, s)
    s->hash = h;
    s->klen = len;
    if (NULL == key) {
	*s->key = '\0';
    } else {
	strncpy(s->key, key, len);
	s->key[len] = '\0';
    }
    s->value = value;
    s->next = *bucket;
    *bucket = s;

    return old;
}

void
pages_init() {
    Mime	m;
    struct _Err	err = ERR_INIT;
    
    memset(&cache, 0, sizeof(struct _Cache));
    cache.root = strdup(".");
    for (m = mime_map; NULL != m->suffix; m++) {
	mime_set(&err, m->suffix, m->type);
    }
}

void
pages_set_root(const char *root) {
    free(cache.root);
    if (NULL == root) {
	cache.root = NULL;
    } else {
	cache.root = strdup(root);
    }
}

static void
page_destroy(Page p) {
    if (NULL != p->resp) {
	text_release(p->resp);
	p->resp = NULL;
    }
    DEBUG_FREE(mem_page_path, p->path);
    DEBUG_FREE(mem_page, p);
    free(p->path);
    free(p);
}

void
pages_cleanup() {
    Slot	*sp = cache.buckets;
    Slot	s;
    Slot	n;
    MimeSlot	*mp = cache.muckets;
    MimeSlot	sm;
    MimeSlot	m;
    int		i;

    for (i = PAGE_BUCKET_SIZE; 0 < i; i--, sp++) {
	for (s = *sp; NULL != s; s = n) {
	    n = s->next;
	    DEBUG_FREE(mem_page_slot, s);
	    page_destroy(s->value);
	    free(s);
	}
	*sp = NULL;
    }
    for (i = MIME_BUCKET_SIZE; 0 < i; i--, mp++) {
	for (sm = *mp; NULL != sm; sm = m) {
	    m = sm->next;
	    DEBUG_FREE(mem_mime_slot, sm);
	    free(sm);
	}
	*mp = NULL;
    }
    free(cache.root);
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

// The page resp points to the page resp msg to save memory and reduce
// allocations.
Page
page_create(const char *path) {
    Page	p = (Page)malloc(sizeof(struct _Page));

    if (NULL != p) {
	DEBUG_ALLOC(mem_page, p);
	p->resp = NULL;
	if (NULL == path) {
	    p->path = NULL;
	} else {
	    p->path = strdup(path);
	    DEBUG_ALLOC(mem_page_path, p->path);
	}
	p->mtime = 0;
	p->last_check = 0.0;
	p->immutable = false;
    }
    return p;
}

Page
page_immutable(Err err, const char *path, const char *content, int clen) {
    Page	p = (Page)malloc(sizeof(struct _Page));
    const char	*mime = path_mime(path);
    long	msize;
    int		cnt;
    int		plen = 0;
    
    if (NULL == p) {
	err_set(err, ERR_MEMORY, "Failed to allocate memory for page.");
	return NULL;     
    }
    DEBUG_ALLOC(mem_page, p);
    if (NULL == path) {
	p->path = NULL;
    } else {
	p->path = strdup(path);
	plen = (int)strlen(path);
	DEBUG_ALLOC(mem_page_path, p->path);
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
    // Format size plus space for the length, the mime type, and some
    // padding. Then add the content length.
    msize = sizeof(page_fmt) + 60 + clen;
    if (NULL == (p->resp = text_allocate((int)msize))) {
	DEBUG_FREE(mem_page, p);
	free(p);
	err_set(err, ERR_MEMORY, "Failed to allocate memory for page content.");
	return NULL;
    }
    cnt = sprintf(p->resp->text, page_fmt, mime, (long)clen);
    msize = cnt + clen;
    memcpy(p->resp->text + cnt, content, clen);
    p->resp->text[msize] = '\0';
    p->resp->len = msize;
    text_ref(p->resp);

    cache_set(path, plen, p);

    return p;
}

static bool
close_return_false(FILE *f) {
    fclose(f);
    return false;
}

static bool
update_contents(Page p) {
    const char	*mime = path_mime(p->path);
    int		plen = (int)strlen(p->path);
    long	size;
    struct stat	fattr;
    long	msize;
    int		cnt;
    struct stat	fs;
    Text	t;
    FILE	*f = fopen(p->path, "rb");
    
    // On linux a directory is opened by fopen (sometimes? all the time?) so
    // fstat is called to get the file mode and verify it is a regular file or
    // a symlink.
    if (NULL != f) {
	fstat(fileno(f), &fs);
	if (!S_ISREG(fs.st_mode) && !S_ISLNK(fs.st_mode)) {
	    fclose(f);
	    f = NULL;
	}
    }
    if (NULL == f) {
	// If not found how about with a /index.html added?
	if (NULL == mime) {
	    char	path[1024];
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

    // Format size plus space for the length, the mime type, and some
    // padding. Then add the content length.
    msize = sizeof(page_fmt) + 60 + size;
    if (NULL == (t = text_allocate((int)msize))) {
	return close_return_false(f);
    }
    cnt = sprintf(t->text, page_fmt, mime, size);
    msize = cnt + size;
    if (0 < size) {
	if (size != (long)fread(t->text + cnt, 1, size, f)) {
	    text_release(t);
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
	text_release(p->resp);
	p->resp = NULL;
    }
    p->resp = t;
    text_ref(p->resp);
    p->last_check = dtime();

    return true;
}

static void
page_remove(Page p) {
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
	    DEBUG_FREE(mem_page_slot, s);
	    page_destroy(s->value);
	    free(s);

	    break;
	}
	prev = s;
    }
}

static Page
page_check(Err err, Page page) {
    if (!page->immutable) {
	double	now = dtime();

	if (page->last_check + PAGE_RECHECK_TIME < now) {
	    struct stat	fattr;

	    if (0 == stat(page->path, &fattr) && page->mtime != fattr.st_mtime) {
		update_contents(page);
		if (NULL == page->resp) {
		    page_remove(page);
		    err_set(err, ERR_NOT_FOUND, "not found.");
		    return NULL;
		}
	    }
	    page->last_check = now;
	}
    }
    return page;
}

Page
page_get(Err err, const char *path, int plen) {
    Page	page;

    if (NULL != strstr(path, "../")) {
	return NULL;
    }
    if (NULL == (page = cache_get(path, plen))) {
	if (NULL != cache.root) {
	    Page	old;
	    char	full_path[2048];
	    char	*s = stpcpy(full_path, cache.root);

	    if ('/' != *cache.root && '/' != *path) {
		*s++ = '/';
	    }
	    if ((int)sizeof(full_path) <= plen + (s - full_path)) {
		err_set(err, ERR_MEMORY, "Failed to allocate memory for page path.");
		return NULL;
	    }
	    strncpy(s, path, plen);
	    s[plen] = '\0';
	    if (NULL == (page = page_create(full_path))) {
		err_set(err, ERR_MEMORY, "Failed to allocate memory for Page.");
		return NULL;
	    }
	    if (!update_contents(page) || NULL == page->resp) {
		page_destroy(page);
		err_set(err, ERR_NOT_FOUND, "not found.");
		return NULL;
	    }
	    if (NULL != (old = cache_set(path, plen, page))) {
		page_destroy(old);
	    }
	}
    } else {
	page = page_check(err, page);
    }
    return page;
}

Page
group_get(Err err, const char *path, int plen) {
    Page	page = NULL;
    Group	g = NULL;
    char	full_path[2048];
    char	*s = NULL;
    Dir		d;

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
	    Page	old;

	    if (NULL == (page = page_create(path))) {
		err_set(err, ERR_MEMORY, "Failed to allocate memory for Page.");
		return NULL;
	    }
	    if (!update_contents(page) || NULL == page->resp) {
		page_destroy(page);
		err_set(err, ERR_NOT_FOUND, "not found.");
		return NULL;
	    }
	    if (NULL != (old = cache_set(path, plen, page))) {
		page_destroy(old);
	    }
	}
	return page;
    }
    return page_check(err, page);
}

Group
group_create(const char *path) {
    Group	g = (Group)malloc(sizeof(struct _Group));

    if (NULL != g) {
	DEBUG_ALLOC(mem_group, g);
	g->next = cache.groups;
	cache.groups = g;
	g->path = strdup(path);
	g->plen = (int)strlen(path);
	DEBUG_ALLOC(mem_group_path, g->path);
	g->dirs = NULL;
    }
    return g;
}

void
group_add(Group g, const char *dir) {
    Dir	d = (Dir)malloc(sizeof(struct _Dir));

    if (NULL != d) {
	DEBUG_ALLOC(mem_dir, d);
	d->next = g->dirs;
	g->dirs = d;
	d->path = strdup(dir);
	d->plen = (int)strlen(dir);
	DEBUG_ALLOC(mem_dir_path, d->path);
    }
}
