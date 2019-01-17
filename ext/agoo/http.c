// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "http.h"

#define BUCKET_SIZE	1024
#define BUCKET_MASK	1023
#define MAX_KEY_UNIQ	9

typedef struct _slot {
    struct _slot	*next;
    const char		*key;
    uint64_t		hash;
    int			klen;
} *Slot;

typedef struct _cache {
    Slot		buckets[BUCKET_SIZE];
} *Cache;

struct _cache		key_cache;

// The rack spec indicates the characters (),/:;<=>?@[]{} are invalid which
// clearly is not consisten with RFC7230 so stick with the RFC.
static char		header_value_chars[256] = "\
xxxxxxxxxxoxxxxxxxxxxxxxxxxxxxxx\
oooooooooooooooooooooooooooooooo\
oooooooooooooooooooooooooooooooo\
ooooooooooooooooooooooooooooooox\
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

static const char	*header_keys[] = {
    "A-IM",
    "ALPN",
    "ARC-Authentication-Results",
    "ARC-Message-Signature",
    "ARC-Seal",
    "Accept",
    "Accept-Additions",
    "Accept-Charset",
    "Accept-Datetime",
    "Accept-Encoding",
    "Accept-Features",
    "Accept-Language",
    "Accept-Language",
    "Accept-Patch",
    "Accept-Post",
    "Accept-Ranges",
    "Access-Control",
    "Access-Control-Allow-Credentials",
    "Access-Control-Allow-Headers",
    "Access-Control-Allow-Methods",
    "Access-Control-Allow-Origin",
    "Access-Control-Max-Age",
    "Access-Control-Request-Headers",
    "Access-Control-Request-Method",
    "Age",
    "Allow",
    "Also-Control",
    "Alt-Svc",
    "Alt-Used",
    "Alternate-Recipient",
    "Alternates",
    "Apparently-To",
    "Apply-To-Redirect-Ref",
    "Approved",
    "Archive",
    "Archived-At",
    "Archived-At",
    "Article-Names",
    "Article-Updates",
    "Authentication-Control",
    "Authentication-Info",
    "Authentication-Results",
    "Authorization",
    "Auto-Submitted",
    "Autoforwarded",
    "Autosubmitted",
    "Base",
    "Bcc",
    "Body",
    "C-Ext",
    "C-Man",
    "C-Opt",
    "C-PEP",
    "C-PEP-Info",
    "Cache-Control",
    "CalDAV-Timezones",
    "Cancel-Key",
    "Cancel-Lock",
    "Cc",
    "Close",
    "Comments",
    "Comments",
    "Compliance",
    "Connection",
    "Content-Alternative",
    "Content-Base",
    "Content-Base",
    "Content-Description",
    "Content-Disposition",
    "Content-Disposition",
    "Content-Duration",
    "Content-Encoding",
    "Content-ID",
    "Content-ID",
    "Content-Identifier",
    "Content-Language",
    "Content-Language",
    "Content-Length",
    "Content-Location",
    "Content-Location",
    "Content-MD5",
    "Content-MD5",
    "Content-Range",
    "Content-Return",
    "Content-Script-Type",
    "Content-Style-Type",
    "Content-Transfer-Encoding",
    "Content-Transfer-Encoding",
    "Content-Translation-Type",
    "Content-Type",
    "Content-Type",
    "Content-Version",
    "Content-features",
    "Control",
    "Conversion",
    "Conversion-With-Loss",
    "Cookie",
    "Cookie2",
    "Cost",
    "DASL",
    "DAV",
    "DKIM-Signature",
    "DL-Expansion-History",
    "Date",
    "Date",
    "Date",
    "Date-Received",
    "Default-Style",
    "Deferred-Delivery",
    "Delivery-Date",
    "Delta-Base",
    "Depth",
    "Derived-From",
    "Destination",
    "Differential-ID",
    "Digest",
    "Discarded-X400-IPMS-Extensions",
    "Discarded-X400-MTS-Extensions",
    "Disclose-Recipients",
    "Disposition-Notification-Options",
    "Disposition-Notification-To",
    "Distribution",
    "Downgraded-Bcc",
    "Downgraded-Cc",
    "Downgraded-Disposition-Notification-To",
    "Downgraded-Final-Recipient",
    "Downgraded-From",
    "Downgraded-In-Reply-To",
    "Downgraded-Mail-From",
    "Downgraded-Message-Id",
    "Downgraded-Original-Recipient",
    "Downgraded-Rcpt-To",
    "Downgraded-References",
    "Downgraded-Reply-To",
    "Downgraded-Resent-Bcc",
    "Downgraded-Resent-Cc",
    "Downgraded-Resent-From",
    "Downgraded-Resent-Reply-To",
    "Downgraded-Resent-Sender",
    "Downgraded-Resent-To",
    "Downgraded-Return-Path",
    "Downgraded-Sender",
    "Downgraded-To",
    "EDIINT-Features",
    "EDIINT-Features",
    "ETag",
    "Eesst-Version",
    "Encoding",
    "Encrypted",
    "Errors-To",
    "Expect",
    "Expires",
    "Expires",
    "Expires",
    "Expiry-Date",
    "Ext",
    "Followup-To",
    "Form-Sub",
    "Forwarded",
    "From",
    "From",
    "From",
    "Generate-Delivery-Report",
    "GetProfile",
    "HTTP2-Settings",
    "Hobareg",
    "Host",
    "IM",
    "If",
    "If-Match",
    "If-Modified-Since",
    "If-None-Match",
    "If-Range",
    "If-Schedule-Tag-Match",
    "If-Unmodified-Since",
    "Importance",
    "In-Reply-To",
    "Incomplete-Copy",
    "Injection-Date",
    "Injection-Info",
    "Jabber-ID",
    "Jabber-ID",
    "Keep-Alive",
    "Keywords",
    "Keywords",
    "Label",
    "Language",
    "Last-Modified",
    "Latest-Delivery-Time",
    "Lines",
    "Link",
    "List-Archive",
    "List-Help",
    "List-ID",
    "List-Owner",
    "List-Post",
    "List-Subscribe",
    "List-Unsubscribe",
    "List-Unsubscribe-Post",
    "Location",
    "Lock-Token",
    "MIME-Version",
    "MIME-Version",
    "MMHS-Acp127-Message-Identifier",
    "MMHS-Authorizing-Users",
    "MMHS-Codress-Message-Indicator",
    "MMHS-Copy-Precedence",
    "MMHS-Exempted-Address",
    "MMHS-Extended-Authorisation-Info",
    "MMHS-Handling-Instructions",
    "MMHS-Message-Instructions",
    "MMHS-Message-Type",
    "MMHS-Originator-PLAD",
    "MMHS-Originator-Reference",
    "MMHS-Other-Recipients-Indicator-CC",
    "MMHS-Other-Recipients-Indicator-To",
    "MMHS-Primary-Precedence",
    "MMHS-Subject-Indicator-Codes",
    "MT-Priority",
    "Man",
    "Max-Forwards",
    "Memento-Datetime",
    "Message-Context",
    "Message-ID",
    "Message-ID",
    "Message-ID",
    "Message-Type",
    "Meter",
    "Method-Check",
    "Method-Check-Expires",
    "NNTP-Posting-Date",
    "NNTP-Posting-Host",
    "Negotiate",
    "Newsgroups",
    "Non-Compliance",
    "Obsoletes",
    "Opt",
    "Optional",
    "Optional-WWW-Authenticate",
    "Ordering-Type",
    "Organization",
    "Organization",
    "Origin",
    "Original-Encoded-Information-Types",
    "Original-From",
    "Original-Message-ID",
    "Original-Recipient",
    "Original-Sender",
    "Original-Subject",
    "Originator-Return-Address",
    "Overwrite",
    "P3P",
    "PEP",
    "PICS-Label",
    "PICS-Label",
    "Path",
    "Pep-Info",
    "Position",
    "Posting-Version",
    "Pragma",
    "Prefer",
    "Preference-Applied",
    "Prevent-NonDelivery-Report",
    "Priority",
    "Privicon",
    "ProfileObject",
    "Protocol",
    "Protocol-Info",
    "Protocol-Query",
    "Protocol-Request",
    "Proxy-Authenticate",
    "Proxy-Authentication-Info",
    "Proxy-Authorization",
    "Proxy-Features",
    "Proxy-Instruction",
    "Public",
    "Public-Key-Pins",
    "Public-Key-Pins-Report-Only",
    "Range",
    "Received",
    "Received-SPF",
    "Redirect-Ref",
    "References",
    "References",
    "Referer",
    "Referer-Root",
    "Relay-Version",
    "Reply-By",
    "Reply-To",
    "Reply-To",
    "Require-Recipient-Valid-Since",
    "Resent-Bcc",
    "Resent-Cc",
    "Resent-Date",
    "Resent-From",
    "Resent-Message-ID",
    "Resent-Reply-To",
    "Resent-Sender",
    "Resent-To",
    "Resolution-Hint",
    "Resolver-Location",
    "Retry-After",
    "Return-Path",
    "SIO-Label",
    "SIO-Label-History",
    "SLUG",
    "Safe",
    "Schedule-Reply",
    "Schedule-Tag",
    "Sec-WebSocket-Accept",
    "Sec-WebSocket-Extensions",
    "Sec-WebSocket-Key",
    "Sec-WebSocket-Protocol",
    "Sec-WebSocket-Version",
    "Security-Scheme",
    "See-Also",
    "Sender",
    "Sender",
    "Sensitivity",
    "Server",
    "Set-Cookie",
    "Set-Cookie2",
    "SetProfile",
    "SoapAction",
    "Solicitation",
    "Status-URI",
    "Strict-Transport-Security",
    "SubOK",
    "Subject",
    "Subject",
    "Subst",
    "Summary",
    "Supersedes",
    "Supersedes",
    "Surrogate-Capability",
    "Surrogate-Control",
    "TCN",
    "TE",
    "TTL",
    "Timeout",
    "Title",
    "To",
    "Topic",
    "Trailer",
    "Transfer-Encoding",
    "UA-Color",
    "UA-Media",
    "UA-Pixels",
    "UA-Resolution",
    "UA-Windowpixels",
    "URI",
    "Upgrade",
    "Urgency",
    "User-Agent",
    "User-Agent",
    "VBR-Info",
    "Variant-Vary",
    "Vary",
    "Version",
    "Via",
    "WWW-Authenticate",
    "Want-Digest",
    "Warning",
    "X-Archived-At",
    "X-Archived-At",
    "X-Content-Type-Options",
    "X-Device-Accept",
    "X-Device-Accept-Charset",
    "X-Device-Accept-Encoding",
    "X-Device-Accept-Language",
    "X-Device-User-Agent",
    "X-Frame-Options",
    "X-Mittente",
    "X-PGP-Sig",
    "X-Ricevuta",
    "X-Riferimento-Message-ID",
    "X-TipoRicevuta",
    "X-Trasporto",
    "X-VerificaSicurezza",
    "X-XSS-Protection",
    "X400-Content-Identifier",
    "X400-Content-Return",
    "X400-Content-Type",
    "X400-MTS-Identifier",
    "X400-Originator",
    "X400-Received",
    "X400-Recipients",
    "X400-Trace",
    "Xref",
    NULL
};

static uint64_t
calc_hash(const char *key, int *lenp) {
    int		len = 0;
    int		klen = *lenp;
    uint64_t	h = 0;
    bool	special = false;

    if (NULL != key) {
	const uint8_t	*k = (const uint8_t*)key;

	for (; len < klen; k++) {
	    // narrow to most used range of 0x4D (77) in size
	    if (*k < 0x2D || 0x7A < *k) {
		special = true;
	    }
	    // fast, just spread it out
	    h = 77 * h + ((*k | 0x20) - 0x2D);
	    len++;
	}
    }
    if (special) {
	*lenp = -len;
    } else {
	*lenp = len;
    }
    return h;
}

static Slot*
get_bucketp(uint64_t h) {
    return key_cache.buckets + (BUCKET_MASK & (h ^ (h << 5) ^ (h >> 7)));
}

static void
key_set(const char *key) {
    int		len = (int)strlen(key);
    int64_t	h = calc_hash(key, &len);
    Slot	*bucket = get_bucketp(h);
    Slot	s;
    
    if (NULL != (s = (Slot)AGOO_MALLOC(sizeof(struct _slot)))) {
	s->hash = h;
	s->klen = len;
	s->key = key;
	s->next = *bucket;
	*bucket = s;
    }
}

void
agoo_http_init() {
    const char	**kp = header_keys;
    
    memset(&key_cache, 0, sizeof(struct _cache));
    for (; NULL != *kp; kp++) {
	key_set(*kp);
    }
}

void
agoo_http_cleanup() {
    Slot	*sp = key_cache.buckets;
    Slot	s;
    Slot	n;
    int		i;

    for (i = BUCKET_SIZE; 0 < i; i--, sp++) {
	for (s = *sp; NULL != s; s = n) {
	    n = s->next;
	    AGOO_FREE(s);
	}
	*sp = NULL;
    }
}

int
agoo_http_header_ok(agooErr err, const char *key, int klen, const char *value, int vlen) {
    int		len = klen;
    int64_t	h = calc_hash(key, &len);
    Slot	*bucket = get_bucketp(h);
    Slot	s;
    bool	found = false;

    for (s = *bucket; NULL != s; s = s->next) {
	if (h == (int64_t)s->hash && len == (int)s->klen &&
	    ((0 <= len && len <= MAX_KEY_UNIQ) || 0 == strncasecmp(s->key, key, klen))) {
	    found = true;
	    break;
	}
    }
    if (!found) {
	char	buf[256];

	if ((int)sizeof(buf) <= klen) {
	    klen = sizeof(buf) - 1;
	}
	strncpy(buf, key, klen);
	buf[klen] = '\0';

	return agoo_err_set(err, AGOO_ERR_ARG, "%s is not a valid HTTP header key.", buf);
    }
    // Now check the value.
    found = false; // reuse as indicator for in a quoted string
    for (; 0 < vlen; vlen--, value++) {
	if ('o' != header_value_chars[(uint8_t)*value]) {
	    return agoo_err_set(err, AGOO_ERR_ARG, "%02x is not a valid HTTP header value character.", *value);
	}
	if ('"' == *value) {
	    found = !found;
	}
    }
    if (found) {
	return agoo_err_set(err, AGOO_ERR_ARG, "HTTP header has unmatched quote.");
    }
    return AGOO_ERR_OK;
}

const char*
agoo_http_code_message(int code) {
    const char	*msg = "";
    
    switch (code) {
    case 100:	msg = "Continue";				break;
    case 101:	msg = "Switching Protocols";			break;
    case 102:	msg = "Processing";				break;
    case 200:	msg = "OK";					break;
    case 201:	msg = "Created";				break;
    case 202:	msg = "Accepted";				break;
    case 203:	msg = "Non-authoritative Information";		break;
    case 204:	msg = "No Content";				break;
    case 205:	msg = "Reset Content";				break;
    case 206:	msg = "Partial Content";			break;
    case 207:	msg = "Multi-Status";				break;
    case 208:	msg = "Already Reported";			break;
    case 226:	msg = "IM Used";				break;
    case 300:	msg = "Multiple Choices";			break;
    case 301:	msg = "Moved Permanently";			break;
    case 302:	msg = "Found";					break;
    case 303:	msg = "See Other";				break;
    case 304:	msg = "Not Modified";				break;
    case 305:	msg = "Use Proxy";				break;
    case 307:	msg = "Temporary Redirect";			break;
    case 308:	msg = "Permanent Redirect";			break;
    case 400:	msg = "Bad Request";				break;
    case 401:	msg = "Unauthorized";				break;
    case 402:	msg = "Payment Required";			break;
    case 403:	msg = "Forbidden";				break;
    case 404:	msg = "Not Found";				break;
    case 405:	msg = "Method Not Allowed";			break;
    case 406:	msg = "Not Acceptable";				break;
    case 407:	msg = "Proxy Authentication Required";		break;
    case 408:	msg = "Request Timeout";			break;
    case 409:	msg = "Conflict";				break;
    case 410:	msg = "Gone";					break;
    case 411:	msg = "Length Required";			break;
    case 412:	msg = "Precondition Failed";			break;
    case 413:	msg = "Payload Too Large";			break;
    case 414:	msg = "Request-URI Too Long";			break;
    case 415:	msg = "Unsupported Media Type";			break;
    case 416:	msg = "Requested Range Not Satisfiable";	break;
    case 417:	msg = "Expectation Failed";			break;
    case 418:	msg = "I'm a teapot";				break;
    case 421:	msg = "Misdirected Request";			break;
    case 422:	msg = "Unprocessable Entity";			break;
    case 423:	msg = "Locked";					break;
    case 424:	msg = "Failed Dependency";			break;
    case 426:	msg = "Upgrade Required";			break;
    case 428:	msg = "Precondition Required";			break;
    case 429:	msg = "Too Many Requests";			break;
    case 431:	msg = "Request Header Fields Too Large";	break;
    case 444:	msg = "Connection Closed Without Response";	break;
    case 451:	msg = "Unavailable For Legal Reasons";		break;
    case 499:	msg = "Client Closed Request";			break;
    case 500:	msg = "Internal Server Error";			break;
    case 501:	msg = "Not Implemented";			break;
    case 502:	msg = "Bad Gateway";				break;
    case 503:	msg = "Service Unavailable";			break;
    case 504:	msg = "Gateway Timeout";			break;
    case 505:	msg = "HTTP Version Not Supported";		break;
    case 506:	msg = "Variant Also Negotiates";		break;
    case 507:	msg = "Insufficient Storage";			break;
    case 508:	msg = "Loop Detected";				break;
    case 510:	msg = "Not Extended";				break;
    case 511:	msg = "Network Authentication Required";	break;
    case 599:	msg = "Network Connect Timeout Error";		break;
    default:							break;
    }
    return msg;
}
