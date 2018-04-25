// Copyright 2016, 2018 by Peter Ohler, All Rights Reserved

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <ruby.h>

#include "debug.h"
#include "dtime.h"
#include "page.h"

#define PAGE_RECHECK_TIME	5.0
#define MAX_KEY_UNIQ		9

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
get_bucketp(Cache cache, uint64_t h) {
    return cache->buckets + (PAGE_BUCKET_MASK & (h ^ (h << 5) ^ (h >> 7)));
}

static MimeSlot*
get_mime_bucketp(Cache cache, uint64_t h) {
    return cache->muckets + (MIME_BUCKET_MASK & (h ^ (h << 5) ^ (h >> 7)));
}

const char*
mime_get(Cache cache, const char *key) {
    int		klen = (int)strlen(key);
    int		len = klen;
    int64_t	h = calc_hash(key, &len);
    MimeSlot	*bucket = get_mime_bucketp(cache, h);
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
cache_get(Cache cache, const char *key, int klen) {
    int		len = klen;
    int64_t	h = calc_hash(key, &len);
    Slot	*bucket = get_bucketp(cache, h);
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

void
mime_set(Cache cache, const char *key, const char *value) {
    int		klen = (int)strlen(key);
    int		len = klen;
    int64_t	h = calc_hash(key, &len);
    MimeSlot	*bucket = get_mime_bucketp(cache, h);
    MimeSlot	s;
    
    if (MAX_MIME_KEY_LEN < len) {
	rb_raise(rb_eArgError, "%s is too long for a file extension. Maximum is %d", key, MAX_MIME_KEY_LEN);
    }
    for (s = *bucket; NULL != s; s = s->next) {
	if (h == (int64_t)s->hash && len == s->klen &&
	    ((0 <= len && len <= MAX_KEY_UNIQ) || 0 == strcmp(s->key, key))) {
	    if (h == (int64_t)s->hash && len == s->klen &&
		((0 <= len && len <= MAX_KEY_UNIQ) || 0 == strcmp(s->key, key))) {
		DEBUG_FREE(mem_mime_slot, s->value)
		free(s->value);
		s->value = strdup(value);
		return;
	    }
	}
    }
    if (NULL == (s = (MimeSlot)malloc(sizeof(struct _MimeSlot)))) {
	rb_raise(rb_eArgError, "out of memory adding %s", key);
    }
    DEBUG_ALLOC(mem_mime_slot, s)
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
}

static Page
cache_set(Cache cache, const char *key, int klen, Page value) {
    int		len = klen;
    int64_t	h = calc_hash(key, &len);
    Slot	*bucket = get_bucketp(cache, h);
    Slot	s;
    Page	old = NULL;
    
    if (MAX_KEY_LEN < len) {
	return value;
    }
    for (s = *bucket; NULL != s; s = s->next) {
	if (h == (int64_t)s->hash && len == s->klen &&
	    ((0 <= len && len <= MAX_KEY_UNIQ) || 0 == strcmp(s->key, key))) {
	    if (h == (int64_t)s->hash && len == s->klen &&
		((0 <= len && len <= MAX_KEY_UNIQ) || 0 == strcmp(s->key, key))) {
		old = s->value;
		// replace
		s->value = value;

		return old;
	    }
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
	strcpy(s->key, key);
    }
    s->value = value;
    s->next = *bucket;
    *bucket = s;

    return old;
}

void
cache_init(Cache cache) {
    Mime	m;

    memset(cache, 0, sizeof(struct _Cache));
    for (m = mime_map; NULL != m->suffix; m++) {
	mime_set(cache, m->suffix, m->type);
    }
}

void
cache_cleanup(Cache cache) {
    Slot	*sp = cache->buckets;
    Slot	s;
    Slot	n;
    MimeSlot	*mp = cache->muckets;
    MimeSlot	sm;
    MimeSlot	m;
    int		i;

    for (i = PAGE_BUCKET_SIZE; 0 < i; i--, sp++) {
	for (s = *sp; NULL != s; s = n) {
	    n = s->next;
	    DEBUG_FREE(mem_page_slot, s)
	    page_destroy(s->value);
	    free(s);
	}
	*sp = NULL;
    }

    for (i = MIME_BUCKET_SIZE; 0 < i; i--, mp++) {
	for (sm = *mp; NULL != sm; sm = m) {
	    m = sm->next;
	    DEBUG_FREE(mem_mime_slot, sm)
	    free(sm);
	}
	*mp = NULL;
    }
}

// The page resp contents point to the page resp msg to save memory and reduce
// allocations.
Page
page_create(const char *path) {
    Page	p = (Page)malloc(sizeof(struct _Page));

    if (NULL != p) {
	DEBUG_ALLOC(mem_page, p)
	p->resp = NULL;
	if (NULL == path) {
	    p->path = NULL;
	} else {
	    p->path = strdup(path);
	    DEBUG_ALLOC(mem_page_path, p->path)
	}
	p->mtime = 0;
	p->last_check = 0.0;
    }
    return p;
}

void
page_destroy(Page p) {
    if (NULL != p->resp) {
	text_release(p->resp);
	p->resp = NULL;
    }
    DEBUG_FREE(mem_page_path, p->path)
	DEBUG_FREE(mem_page, p)
    free(p->path);
    free(p);
}

#define FOO 1

static bool
update_contents(Cache cache, Page p) {
    const char	*mime = NULL;
    int		plen = (int)strlen(p->path);
    const char	*suffix = p->path + plen - 1;
    FILE	*f;
    long	size;
    struct stat	fattr;
    long	msize;
    int		cnt;
    struct stat	fs;
    Text	t;
    
    for (; '.' != *suffix; suffix--) {
	if (suffix <= p->path) {
	    suffix = NULL;
	    break;
	}
    }
    if (NULL != suffix) {
	suffix++;
	if (NULL == (mime = mime_get(cache, suffix))) {
	    mime = "text/plain";
	}
    } else {
	mime = "text/plain";
    }

    f = fopen(p->path, "rb");
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
	if (NULL == suffix) {
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
	} else {
	    return false;
	}
    }
    if (0 != fseek(f, 0, SEEK_END)) {
	fclose(f);
	return false;
    }
    if (0 >= (size = ftell(f))) {
	fclose(f);
	return false;
    }
    rewind(f);
    // Format size plus space for the length, the mime type, and some
    // padding. Then add the content length.
    msize = sizeof(page_fmt) + 60 + size;
    if (NULL == (t = text_allocate(msize))) {
	return false;
    }
    cnt = sprintf(t->text, page_fmt, mime, size);
    msize = cnt + size;
    if (size != (long)fread(t->text + cnt, 1, size, f)) {
	fclose(f);
	text_release(t);
	return false;
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

Page
page_get(Err err, Cache cache, const char *dir, const char *path, int plen) {
    Page	page;

    if (NULL == (page = cache_get(cache, path, plen))) {
	Page	old;
	char	full_path[2048];
	char	*s = stpcpy(full_path, dir);

	if ('/' != *dir && '/' != *path) {
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
	if (!update_contents(cache, page) || NULL == page->resp) {
	    page_destroy(page);
	    err_set(err, ERR_NOT_FOUND, "not found.");
	    return NULL;
	}
	if (NULL != (old = cache_set(cache, path, plen, page))) {
	    page_destroy(old);
	}
    } else {
	double	now = dtime();

	if (page->last_check + PAGE_RECHECK_TIME < now) {
	    struct stat	fattr;

	    if (0 == stat(page->path, &fattr) && page->mtime != fattr.st_mtime) {
		update_contents(cache, page);
		if (NULL == page->resp) {
		    page_destroy(page);
		    err_set(err, ERR_NOT_FOUND, "not found.");
		    return NULL;
		}
	    }
	    page->last_check = now;
	}
    }
    return page;
}
