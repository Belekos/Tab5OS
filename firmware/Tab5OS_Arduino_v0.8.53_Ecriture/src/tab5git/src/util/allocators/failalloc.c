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

#include "failalloc.h"

void *git_failalloc_malloc(size_t len, const char *file, int line)
{
	GIT_UNUSED(len);
	GIT_UNUSED(file);
	GIT_UNUSED(line);

	return NULL;
}

void *git_failalloc_realloc(void *ptr, size_t size, const char *file, int line)
{
	GIT_UNUSED(ptr);
	GIT_UNUSED(size);
	GIT_UNUSED(file);
	GIT_UNUSED(line);

	return NULL;
}

void git_failalloc_free(void *ptr)
{
	GIT_UNUSED(ptr);
}
