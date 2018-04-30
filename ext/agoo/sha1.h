#ifndef __AGOO_SHA1_H__
#define __AGOO_SHA1_H__

#include <stdint.h>

#define SHA1_DIGEST_SIZE 20

extern void sha1(const uint8_t *data, size_t len, uint8_t *digest);

#endif // __AGOO_SHA1_H__
