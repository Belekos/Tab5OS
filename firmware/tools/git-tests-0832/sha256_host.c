/* Host-only SHA-256 adapter. Production keeps the ESP mbedTLS implementation. */
#include "hash.h"
#include <string.h>
int git_hash_sha256_global_init(void) { return 0; }
int git_hash_sha256_ctx_init(git_hash_sha256_ctx *c) { return SHA256_Init(&c->c) == 1 ? 0 : -1; }
void git_hash_sha256_ctx_cleanup(git_hash_sha256_ctx *c) { if (c) memset(c, 0, sizeof(*c)); }
int git_hash_sha256_init(git_hash_sha256_ctx *c) { return SHA256_Init(&c->c) == 1 ? 0 : -1; }
int git_hash_sha256_update(git_hash_sha256_ctx *c, const void *data, size_t n) { return SHA256_Update(&c->c, data, n) == 1 ? 0 : -1; }
int git_hash_sha256_final(unsigned char *out, git_hash_sha256_ctx *c) { return SHA256_Final(out, &c->c) == 1 ? 0 : -1; }
