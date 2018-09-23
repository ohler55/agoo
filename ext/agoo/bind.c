// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <netdb.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "bind.h"
#include "debug.h"
#include "log.h"

Bind
bind_port(Err err, int port) {
    Bind	b = (Bind)malloc(sizeof(struct _Bind));

    if (NULL != b) {
	char	id[1024];
	
	DEBUG_ALLOC(mem_bind, b);
	memset(b, 0, sizeof(struct _Bind));
	b->port = port;
	b->family = AF_INET;
	snprintf(id, sizeof(id) - 1, "http://:%d", port);
	strcpy(b->scheme, "http");
	b->id = strdup(id);
	b->kind = CON_HTTP;
	b->read = NULL;
	b->write = NULL;
	b->events = NULL;
    }
    return b;
}

static Bind
url_tcp(Err err, const char *url, const char *scheme) {
    char		*colon = index(url, ':');
    struct in_addr	addr = { .s_addr = 0 };
    int			port;
    Bind		b;
    
    if (NULL == colon) {
	port = 80;
    } else if (15 < colon - url) {
	err_set(err, ERR_ARG, "%s bind address is not valid, too long. (%s)", scheme, url);
	return NULL;
    } else if (':' == *url) {
	port = atoi(colon + 1);
    } else {
	char	buf[32];

	strncpy(buf, url, colon - url);
	buf[colon - url] = '\0';
	if (0 == inet_aton(buf, &addr)) {
	    err_set(err, ERR_ARG, "%s bind address is not valid. (%s)", scheme, url);
	    return NULL;
	}
	port = atoi(colon + 1);
    }
    if (NULL != (b = (Bind)malloc(sizeof(struct _Bind)))) {
	char	id[64];
	
	DEBUG_ALLOC(mem_bind, b);
	memset(b, 0, sizeof(struct _Bind));

	b->port = port;
	b->addr4 = addr;
	b->family = AF_INET;
	snprintf(id, sizeof(id), "%s://%s:%d", scheme, inet_ntoa(addr), port);
	b->id = strdup(id);
	strncpy(b->scheme, scheme, sizeof(b->scheme));
	b->scheme[sizeof(b->scheme) - 1] = '\0';
	b->kind = CON_HTTP;
	b->read = NULL;
	b->write = NULL;
	b->events = NULL;

	return b;
    }
    err_set(err, ERR_MEMORY, "Failed to allocate memory for a Bind.");
    
    return b;
}

static Bind
url_tcp6(Err err, const char *url, const char *scheme) {
    struct in6_addr	addr;
    char		*end = index(url, ']');
    int			port = 80;
    char		buf[256];
    Bind		b;
    
    if (':' == *(end + 1)) {
	port = atoi(end + 2);
    }
    memcpy(buf, url + 1, end - url - 1);
    buf[end - url - 1] = '\0';
    memset(&addr, 0, sizeof(addr));
    if (0 == inet_pton(AF_INET6, buf, &addr)) {
	err_set(err, ERR_ARG, "%s bind address is not valid. (%s)", scheme, url);
	return NULL;
    }
    if (NULL != (b = (Bind)malloc(sizeof(struct _Bind)))) {
	char	str[INET6_ADDRSTRLEN + 1];
	
	DEBUG_ALLOC(mem_bind, b);
	memset(b, 0, sizeof(struct _Bind));

	b->port = port;
	b->addr6 = addr;
	b->family = AF_INET6;
	snprintf(buf, sizeof(buf), "%s://[%s]:%d", scheme, inet_ntop(AF_INET6, &addr, str, INET6_ADDRSTRLEN), port);
	b->id = strdup(buf);
	strncpy(b->scheme, scheme, sizeof(b->scheme));
	b->scheme[sizeof(b->scheme) - 1] = '\0';
	b->kind = CON_HTTP;
	b->read = NULL;
	b->write = NULL;
	b->events = NULL;

	return b;
    }
    err_set(err, ERR_MEMORY, "Failed to allocate memory for a Bind.");
    
    return b;
}

static Bind
url_named(Err err, const char *url) {
    if ('\0' == *url) {
	err_set(err, ERR_ARG, "Named Unix sockets names must not be empty.");
	return NULL;
    } else {
	Bind	b = (Bind)malloc(sizeof(struct _Bind));

	if (NULL != b) {
	    const char	*fmt = "unix://%s";
	    char	id[1024];
	
	    DEBUG_ALLOC(mem_bind, b);
	    memset(b, 0, sizeof(struct _Bind));
	    b->name = strdup(url);
	    snprintf(id, sizeof(id) - 1, fmt, url);
	    b->id = strdup(id);
	    strcpy(b->scheme, "unix");
	    b->kind = CON_HTTP;
	    b->read = NULL;
	    b->write = NULL;
	    b->events = NULL;
	}
	return b;
    }
    err_set(err, ERR_MEMORY, "Failed to allocate memory for a Bind.");

    return NULL;
}

static Bind
url_ssl(Err err, const char *url) {
    // TBD
    return NULL;
}

Bind
bind_url(Err err, const char *url) {
    if (0 == strncmp("tcp://", url, 6)) {
	if ('[' == url[6]) {
	    return url_tcp6(err, url + 6, "tcp");
	}
	return url_tcp(err, url + 6, "tcp");
    }
    if (0 == strncmp("http://", url, 7)) {
	if ('[' == url[7]) {
	    return url_tcp6(err, url + 7, "http");
	}
	return url_tcp(err, url + 7, "http");
    }
    if (0 == strncmp("unix://", url, 7)) {
	return url_named(err, url + 7);
    }
    if (0 == strncmp("https://", url, 8)) {
	return url_ssl(err, url + 8);
    }
    if (0 == strncmp("ssl://", url, 6)) {
	return url_ssl(err, url + 6);
    }
    // All others assume
    {
	char	*colon = index(url, ':');
	char	scheme[8];

	if (NULL != colon && colon - url < (int)sizeof(scheme)) {
	    int	slen = colon - url;

	    memcpy(scheme, url, slen);
	    scheme[slen] = '\0';
	    if ('[' == url[slen + 3]) {
		return url_tcp6(err, url + slen + 3, scheme);
	    }
	    return url_tcp(err, url + slen + 3, scheme);
	}
    }
    return NULL;
}

void
bind_destroy(Bind b) {
    DEBUG_FREE(mem_bind, b);
    free(b->id);
    free(b->name);
    free(b->key);
    free(b->cert);
    free(b->ca);
    free(b);
}

static int
usual_listen(Err err, Bind b) {
    int		optval = 1;
    int	domain = PF_INET;

    if (AF_INET6 == b->family) {
	domain = PF_INET6;
    }
    if (0 >= (b->fd = socket(domain, SOCK_STREAM, IPPROTO_TCP))) {
	log_cat(&error_cat, "Server failed to open server socket on port %d. %s.", b->port, strerror(errno));

	return err_set(err, errno, "Server failed to open server socket. %s.", strerror(errno));
    }
#ifdef OSX_OS 
    setsockopt(b->fd, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval));
#endif    
    setsockopt(b->fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    setsockopt(b->fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
    if (AF_INET6 == b->family) {
	struct sockaddr_in6	addr;

	memset(&addr, 0, sizeof(addr));
	addr.sin6_flowinfo = 0;
	addr.sin6_family = b->family;
	addr.sin6_addr = b->addr6;
	addr.sin6_port = htons(b->port);
	if (0 > bind(b->fd, (struct sockaddr*)&addr, sizeof(addr))) {
	    log_cat(&error_cat, "Server failed to bind server socket. %s.", strerror(errno));

	    return err_set(err, errno, "Server failed to bind server socket. %s.", strerror(errno));
	}
    } else {
	struct sockaddr_in	addr;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = b->family;
	addr.sin_addr = b->addr4;
	addr.sin_port = htons(b->port);
	if (0 > bind(b->fd, (struct sockaddr*)&addr, sizeof(addr))) {
	    log_cat(&error_cat, "Server failed to bind server socket. %s.", strerror(errno));

	    return err_set(err, errno, "Server failed to bind server socket. %s.", strerror(errno));
	}
    }
    listen(b->fd, 1000);

    return ERR_OK;
}

static int
named_listen(Err err, Bind b) {
    struct sockaddr_un	addr;

    remove(b->name);
    if (0 >= (b->fd = socket(AF_UNIX, SOCK_STREAM, 0))) {
	log_cat(&error_cat, "Server failed to open server socket on %s. %s.", b->name, strerror(errno));

	return err_set(err, errno, "Server failed to open server socket on %s. %s.", b->name, strerror(errno));
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, b->name);
    if (0 > bind(b->fd, (struct sockaddr*)&addr, sizeof(addr))) {
	log_cat(&error_cat, "Server failed to bind server socket. %s.", strerror(errno));

	return err_set(err, errno, "Server failed to bind server socket. %s.", strerror(errno));
    }
    listen(b->fd, 100);

    return ERR_OK;
}

static int
ssl_listen(Err err, Bind b) {
    // TBD
    return ERR_OK;
}

int
bind_listen(Err err, Bind b) {
    if (NULL != b->name) {
	return named_listen(err, b);
    }
    if (NULL != b->key) {
	return ssl_listen(err, b);
    }
    return usual_listen(err, b);
}

void
bind_close(Bind b) {
    if (0 != b->fd) {
	close(b->fd);
	b->fd = 0;
    }
}
