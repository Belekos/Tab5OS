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

#include <stdio.h>

#include "git2_util.h"
#include "vector.h"
#include "strlist.h"

int git_strlist_copy(char ***out, const char **in, size_t len)
{
	char **dup;
	size_t i;

	dup = git__calloc(len, sizeof(char *));
	GIT_ERROR_CHECK_ALLOC(dup);

	for (i = 0; i < len; i++) {
		dup[i] = git__strdup(in[i]);
		GIT_ERROR_CHECK_ALLOC(dup[i]);
	}

	*out = dup;
	return 0;
}

int git_strlist_copy_with_null(char ***out, const char **in, size_t len)
{
	char **dup;
	size_t new_len, i;

	GIT_ERROR_CHECK_ALLOC_ADD(&new_len, len, 1);

	dup = git__calloc(new_len, sizeof(char *));
	GIT_ERROR_CHECK_ALLOC(dup);

	for (i = 0; i < len; i++) {
		dup[i] = git__strdup(in[i]);
		GIT_ERROR_CHECK_ALLOC(dup[i]);
	}

	*out = dup;
	return 0;
}

bool git_strlist_contains_prefix(
	const char **strings,
	size_t len,
	const char *str,
	size_t n)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (strncmp(strings[i], str, n) == 0)
			return true;
	}

	return false;
}

bool git_strlist_contains_key(
	const char **strings,
	size_t len,
	const char *key,
	char delimiter)
{
	const char *c;

	for (c = key; *c; c++) {
		if (*c == delimiter)
			break;
	}

	return *c ?
	       git_strlist_contains_prefix(strings, len, key, (c - key)) :
	       false;
}

void git_strlist_free(char **strings, size_t len)
{
	size_t i;

	if (!strings)
		return;

	for (i = 0; i < len; i++)
		git__free(strings[i]);

	git__free(strings);
}

void git_strlist_free_with_null(char **strings)
{
	char **s;

	if (!strings)
		return;

	for (s = strings; *s; s++)
		git__free(*s);

	git__free(strings);
}
