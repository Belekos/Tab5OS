/* Arduino translation unit; upstream license follows. */
#include "../../../Tab5GitEmbeddedConfig.h"
#ifndef PCRE2_EXPORT
#define PCRE2_EXPORT 
#endif
#ifndef PCRE2_EXP_DECL
#define PCRE2_EXP_DECL 
#endif
#ifndef PCRE2_EXP_DEFN
#define PCRE2_EXP_DEFN 
#endif
#ifndef PCRE2_STATIC
#define PCRE2_STATIC 1
#endif
#ifndef SHA1DC_CUSTOM_INCLUDE_SHA1_C
#define SHA1DC_CUSTOM_INCLUDE_SHA1_C "../git2_util.h"
#endif
#ifndef SHA1DC_CUSTOM_INCLUDE_UBC_CHECK_C
#define SHA1DC_CUSTOM_INCLUDE_UBC_CHECK_C "../git2_util.h"
#endif
#ifndef SHA1DC_NO_STANDARD_INCLUDES
#define SHA1DC_NO_STANDARD_INCLUDES 1
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#ifndef NDEBUG
#define NDEBUG 1
#endif
/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "collisiondetect.h"

int git_hash_sha1_global_init(void)
{
	return 0;
}

int git_hash_sha1_ctx_init(git_hash_sha1_ctx *ctx)
{
	return git_hash_sha1_init(ctx);
}

void git_hash_sha1_ctx_cleanup(git_hash_sha1_ctx *ctx)
{
	GIT_UNUSED(ctx);
}

int git_hash_sha1_init(git_hash_sha1_ctx *ctx)
{
	GIT_ASSERT_ARG(ctx);
	SHA1DCInit(&ctx->c);
	return 0;
}

int git_hash_sha1_update(git_hash_sha1_ctx *ctx, const void *data, size_t len)
{
	GIT_ASSERT_ARG(ctx);
	SHA1DCUpdate(&ctx->c, data, len);
	return 0;
}

int git_hash_sha1_final(unsigned char *out, git_hash_sha1_ctx *ctx)
{
	GIT_ASSERT_ARG(ctx);
	if (SHA1DCFinal(out, &ctx->c)) {
		git_error_set(GIT_ERROR_SHA, "SHA1 collision attack detected");
		return -1;
	}

	return 0;
}
