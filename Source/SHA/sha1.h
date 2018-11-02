/* public api for steve reid's public domain SHA-1 implementation */
/* this file is in the public domain */

#ifndef __SHA1_H
#define __SHA1_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned state[5];
    unsigned count[2];
    unsigned char  buffer[64];
} SHA1_CTX;

#define SHA1_DIGEST_SIZE 20

void _SHA1_Init(SHA1_CTX* context);
void _SHA1_Update(SHA1_CTX* context, const unsigned char* data, const size_t len);
void _SHA1_Final(SHA1_CTX* context, unsigned char digest[SHA1_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* __SHA1_H */
