/* Arduino translation unit; upstream license follows. */
#include "../../Tab5GitEmbeddedConfig.h"
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
#define SHA1DC_CUSTOM_INCLUDE_SHA1_C "git2_util.h"
#endif
#ifndef SHA1DC_CUSTOM_INCLUDE_UBC_CHECK_C
#define SHA1DC_CUSTOM_INCLUDE_UBC_CHECK_C "git2_util.h"
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

#include "alloc.h"
#include "runtime.h"

#include "allocators/stdalloc.h"
#include "allocators/debugalloc.h"
#include "allocators/failalloc.h"
#include "allocators/win32_leakcheck.h"

/* Fail any allocation until git_libgit2_init is called. */
git_allocator git__allocator = {
	git_failalloc_malloc,
	git_failalloc_realloc,
	git_failalloc_free
};

void *git__calloc(size_t nelem, size_t elsize)
{
	size_t newsize;
	void *ptr;

	if (GIT_MULTIPLY_SIZET_OVERFLOW(&newsize, nelem, elsize))
		return NULL;

	if ((ptr = git__malloc(newsize)))
		memset(ptr, 0, newsize);

	return ptr;
}

void *git__reallocarray(void *ptr, size_t nelem, size_t elsize)
{
	size_t newsize;

	if (GIT_MULTIPLY_SIZET_OVERFLOW(&newsize, nelem, elsize))
		return NULL;

	return git__realloc(ptr, newsize);
}

void *git__mallocarray(size_t nelem, size_t elsize)
{
	return git__reallocarray(NULL, nelem, elsize);
}

char *git__strdup(const char *str)
{
	size_t len = strlen(str) + 1;
	void *ptr = git__malloc(len);

	if (ptr)
		memcpy(ptr, str, len);

	return ptr;
}

char *git__strndup(const char *str, size_t n)
{
	size_t len = p_strnlen(str, n);
	char *ptr = git__malloc(len + 1);

	if (ptr) {
		memcpy(ptr, str, len);
		ptr[len] = '\0';
	}

	return ptr;
}

char *git__substrdup(const char *str, size_t n)
{
	char *ptr = git__malloc(n + 1);

	if (ptr) {
		memcpy(ptr, str, n);
		ptr[n] = '\0';
	}

	return ptr;
}

static int setup_default_allocator(void)
{
#if defined(GIT_WIN32_LEAKCHECK)
	return git_win32_leakcheck_init_allocator(&git__allocator);
#elif defined(GIT_DEBUG_STRICT_ALLOC)
	return git_debugalloc_init_allocator(&git__allocator);
#else
	return git_stdalloc_init_allocator(&git__allocator);
#endif
}

int git_allocator_global_init(void)
{
	/*
	 * We don't want to overwrite any allocator which has been set
	 * before the init function is called.
	 */
	if (git__allocator.gmalloc != git_failalloc_malloc)
		return 0;

	return setup_default_allocator();
}

int git_allocator_setup(git_allocator *allocator)
{
	if (!allocator)
		return setup_default_allocator();

	memcpy(&git__allocator, allocator, sizeof(*allocator));
	return 0;
}
