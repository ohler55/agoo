// Taken and modified from the original by Steve Reid which is in the public
// domain. It has been modified by James H. Brown, Saul Kravitz, Ralph Giles,
// and now Peter Ohler.

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "sha1.h"

typedef struct {
    uint32_t    h0;
    uint32_t    h1;
    uint32_t    h2;
    uint32_t    h3;
    uint32_t    h4;
    uint32_t    count[2];
    uint8_t     buffer[64];
} Ctx;

// TBD make this stuff ENDIAN independent. Now it is little endian only.
#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))
#define blk0(i) (block->l[i] = (rol(block->l[i],24)&0xFF00FF00)|(rol(block->l[i],8)&0x00FF00FF))
#define blk(i) (block->l[i&0x0F] = rol(block->l[(i+13)&0x0F]^block->l[(i+8)&0x0F]^block->l[(i+2)&0x0F]^block->l[i&0x0F],1))

#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);

// Transform a 512 bit block.
static void
transform(Ctx *ctx, const uint8_t buffer[64]) {
    uint32_t    a = ctx->h0;
    uint32_t    b = ctx->h1;
    uint32_t    c = ctx->h2;
    uint32_t    d = ctx->h3;
    uint32_t    e = ctx->h4;

    typedef union {
        uint8_t c[64];
        uint32_t l[16];
    } CHAR64LONG16;
    CHAR64LONG16* block;

    block = (CHAR64LONG16*)buffer;

    /* 4 rounds of 20 operations each. Loop unrolled. */
    R0(a,b,c,d,e, 0); R0(e,a,b,c,d, 1); R0(d,e,a,b,c, 2); R0(c,d,e,a,b, 3);
    R0(b,c,d,e,a, 4); R0(a,b,c,d,e, 5); R0(e,a,b,c,d, 6); R0(d,e,a,b,c, 7);
    R0(c,d,e,a,b, 8); R0(b,c,d,e,a, 9); R0(a,b,c,d,e,10); R0(e,a,b,c,d,11);
    R0(d,e,a,b,c,12); R0(c,d,e,a,b,13); R0(b,c,d,e,a,14); R0(a,b,c,d,e,15);
    R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);
    R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
    R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
    R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
    R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
    R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);
    R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
    R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
    R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
    R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
    R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);
    R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
    R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
    R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
    R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
    R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);

    ctx->h0 += a;
    ctx->h1 += b;
    ctx->h2 += c;
    ctx->h3 += d;
    ctx->h4 += e;
}

static void
update(Ctx *ctx, const uint8_t* data, const size_t len) {
    size_t      i;
    size_t      j = (ctx->count[0] >> 3) & 0x3F;

    if ((ctx->count[0] += len << 3) < (len << 3)) {
        ctx->count[1]++;
    }
    ctx->count[1] += (len >> 29);
    if ((j + len) > 63) {
        i = 64 - j;
        memcpy(ctx->buffer + j, data, i);
        transform(ctx, ctx->buffer);
        for (; i + 63 < len; i += 64) {
            transform(ctx, data + i);
        }
        j = 0;
    } else {
        i = 0;
    }
    memcpy(ctx->buffer + j, data + i, len - i);
}

void
sha1(const uint8_t *data, size_t len, uint8_t *digest) {
    Ctx		ctx;
    uint32_t    i;
    uint8_t     finalcount[8];
    int         shift;

    ctx.h0 = 0x67452301;
    ctx.h1 = 0xEFCDAB89;
    ctx.h2 = 0x98BADCFE;
    ctx.h3 = 0x10325476;
    ctx.h4 = 0xC3D2E1F0;
    ctx.count[0] = 0;
    ctx.count[1] = 0;

    update(&ctx, data, len);

    for (i = 0; i < 8; i++) {
        finalcount[i] = (uint8_t)((ctx.count[(i >= 4 ? 0 : 1)] >> ((3-(i & 3)) * 8) ) & 0xFF);
    }
    update(&ctx, (uint8_t*)"\x80", 1);
    while ((ctx.count[0] & 0x000001F8) != 0x000001C0) {
        update(&ctx, (uint8_t*)"", 1);
    }
    update(&ctx, finalcount, 8);
    for (i = 0; i < SHA1_DIGEST_SIZE; i++) {
        shift = (3 - (i & 3)) * 8;
        switch (i >> 2) {
        case 0:
            digest[i] = (uint8_t)((ctx.h0 >> shift) & 0xFF);
            break;
        case 1:
            digest[i] = (uint8_t)((ctx.h1 >> shift) & 0xFF);
            break;
        case 2:
            digest[i] = (uint8_t)((ctx.h2 >> shift) & 0xFF);
            break;
        case 3:
            digest[i] = (uint8_t)((ctx.h3 >> shift) & 0xFF);
            break;
        case 4:
            digest[i] = (uint8_t)((ctx.h4 >> shift) & 0xFF);
            break;
        default:
            break;
        }
    }
}
