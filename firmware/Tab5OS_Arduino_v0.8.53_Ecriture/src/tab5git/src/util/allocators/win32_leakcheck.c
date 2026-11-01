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

#include "win32_leakcheck.h"

#if defined(GIT_WIN32_LEAKCHECK)

#include "../win32/w32_leakcheck.h"

static void *leakcheck_malloc(size_t len, const char *file, int line)
{
	void *ptr = _malloc_dbg(len, _NORMAL_BLOCK, git_win32_leakcheck_stacktrace(1,file), line);
	if (!ptr) git_error_set_oom();
	return ptr;
}

static void *leakcheck_realloc(void *ptr, size_t size, const char *file, int line)
{
	void *new_ptr = _realloc_dbg(ptr, size, _NORMAL_BLOCK, git_win32_leakcheck_stacktrace(1,file), line);
	if (!new_ptr) git_error_set_oom();
	return new_ptr;
}

static void leakcheck_free(void *ptr)
{
	free(ptr);
}

int git_win32_leakcheck_init_allocator(git_allocator *allocator)
{
	allocator->gmalloc = leakcheck_malloc;
	allocator->grealloc = leakcheck_realloc;
	allocator->gfree = leakcheck_free;
	return 0;
}

#else

int git_win32_leakcheck_init_allocator(git_allocator *allocator)
{
	GIT_UNUSED(allocator);
	git_error_set(GIT_EINVALID, "leakcheck memory allocator not available");
	return -1;
}

#endif
