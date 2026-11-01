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

#include "regexp.h"

#if defined(GIT_REGEX_PCRE)

int git_regexp_compile(git_regexp *r, const char *pattern, int flags)
{
	int erroffset, cflags = 0;
	const char *error = NULL;

	if (flags & GIT_REGEXP_ICASE)
		cflags |= PCRE_CASELESS;

	if ((*r = pcre_compile(pattern, cflags, &error, &erroffset, NULL)) == NULL) {
		git_error_set_str(GIT_ERROR_REGEX, error);
		return GIT_EINVALIDSPEC;
	}

	return 0;
}

void git_regexp_dispose(git_regexp *r)
{
	pcre_free(*r);
	*r = NULL;
}

int git_regexp_match(const git_regexp *r, const char *string)
{
	int error;
	if ((error = pcre_exec(*r, NULL, string, (int) strlen(string), 0, 0, NULL, 0)) < 0)
		return (error == PCRE_ERROR_NOMATCH) ? GIT_ENOTFOUND : GIT_EINVALIDSPEC;
	return 0;
}

int git_regexp_search(const git_regexp *r, const char *string, size_t nmatches, git_regmatch *matches)
{
	int static_ovec[9] = {0}, *ovec;
	int error;
	size_t i;

	/* The ovec array always needs to be a multiple of three */
	if (nmatches <= ARRAY_SIZE(static_ovec) / 3)
		ovec = static_ovec;
	else
		ovec = git__calloc(nmatches * 3, sizeof(*ovec));
	GIT_ERROR_CHECK_ALLOC(ovec);

	if ((error = pcre_exec(*r, NULL, string, (int) strlen(string), 0, 0, ovec, (int) nmatches * 3)) < 0)
		goto out;

	if (error == 0)
		error = (int) nmatches;

	for (i = 0; i < (unsigned int) error; i++) {
		matches[i].start = (ovec[i * 2] < 0) ? -1 : ovec[i * 2];
		matches[i].end = (ovec[i * 2 + 1] < 0) ? -1 : ovec[i * 2 + 1];
	}
	for (i = (unsigned int) error; i < nmatches; i++)
		matches[i].start = matches[i].end = -1;

out:
	if (nmatches > ARRAY_SIZE(static_ovec) / 3)
		git__free(ovec);
	if (error < 0)
		return (error == PCRE_ERROR_NOMATCH) ? GIT_ENOTFOUND : GIT_EINVALIDSPEC;
	return 0;
}

#elif defined(GIT_REGEX_BUILTIN) || defined(GIT_REGEX_PCRE2)

/* Keep every PCRE2 allocation inside libgit2's installed quota, including
 * match frames. A compiled pattern copies these allocator callbacks. */
static void *regexp_alloc(PCRE2_SIZE size, void *payload)
{
	GIT_UNUSED(payload);
	return git__malloc(size);
}

static void regexp_free(void *pointer, void *payload)
{
	GIT_UNUSED(payload);
	git__free(pointer);
}

#if defined(ESP_PLATFORM) || defined(TAB5_GIT_EMBEDDED_PROFILE)
extern int tab5_git_cancelled(void);
static int regexp_callout(pcre2_callout_block *block, void *payload)
{
	GIT_UNUSED(block);
	GIT_UNUSED(payload);
	return tab5_git_cancelled() ? PCRE2_ERROR_CALLOUT : 0;
}
#endif

static pcre2_match_context *regexp_match_context(pcre2_general_context *general)
{
	pcre2_match_context *context = pcre2_match_context_create(general);
	if (!context)
		return NULL;
	/* Heap units are KiB; these also bound hostile repository patterns. */
	pcre2_set_heap_limit(context, 256);
	pcre2_set_depth_limit(context, 128);
	pcre2_set_match_limit(context, 100000);
#if defined(ESP_PLATFORM) || defined(TAB5_GIT_EMBEDDED_PROFILE)
	pcre2_set_callout(context, regexp_callout, NULL);
#endif
	return context;
}

int git_regexp_compile(git_regexp *r, const char *pattern, int flags)
{
	unsigned char errmsg[256];
	PCRE2_SIZE erroff;
	int error, cflags = 0;
	pcre2_general_context *general;
	pcre2_compile_context *context;
	*r = NULL;
	general = pcre2_general_context_create(regexp_alloc, regexp_free, NULL);
	if (!general)
		return -1;
	context = pcre2_compile_context_create(general);
	if (!context) {
		pcre2_general_context_free(general);
		return -1;
	}
	pcre2_set_parens_nest_limit(context, 16);
	pcre2_set_max_pattern_length(context, 4096);

	if (flags & GIT_REGEXP_ICASE)
		cflags |= PCRE2_CASELESS;
#if defined(ESP_PLATFORM) || defined(TAB5_GIT_EMBEDDED_PROFILE)
	cflags |= PCRE2_AUTO_CALLOUT;
#endif

	*r = pcre2_compile((const unsigned char *) pattern,
			PCRE2_ZERO_TERMINATED,
			cflags, &error, &erroff, context);
	pcre2_compile_context_free(context);
	pcre2_general_context_free(general);
	if (!*r) {
		pcre2_get_error_message(error, errmsg, sizeof(errmsg));
		git_error_set_str(GIT_ERROR_REGEX, (char *) errmsg);
		return GIT_EINVALIDSPEC;
	}

	return 0;
}

void git_regexp_dispose(git_regexp *r)
{
	pcre2_code_free(*r);
	*r = NULL;
}

int git_regexp_match(const git_regexp *r, const char *string)
{
	pcre2_match_data *data;
	pcre2_general_context *general;
	pcre2_match_context *context;
	int error;

	general = pcre2_general_context_create(regexp_alloc, regexp_free, NULL);
	if (!general)
		return -1;
	context = regexp_match_context(general);
	data = context ? pcre2_match_data_create(1, general) : NULL;
	pcre2_general_context_free(general);
	if (!data) {
		pcre2_match_context_free(context);
		return -1;
	}

	error = pcre2_match(*r, (const unsigned char *) string, strlen(string), 0, 0, data, context);
	pcre2_match_data_free(data);
	pcre2_match_context_free(context);

	if (error < 0)
		return (error == PCRE2_ERROR_NOMATCH) ? GIT_ENOTFOUND : GIT_EINVALIDSPEC;

	return 0;
}

int git_regexp_search(
	const git_regexp *r,
	const char *string,
	size_t given_nmatches,
	git_regmatch *matches)
{
	pcre2_match_data *data = NULL;
	pcre2_general_context *general;
	pcre2_match_context *context;
	PCRE2_SIZE *ovec;
	size_t i, captures;
	uint32_t nmatches;
	int error;

	GIT_ASSERT(given_nmatches <= UINT32_MAX);

	nmatches = (uint32_t) given_nmatches;

	general = pcre2_general_context_create(regexp_alloc, regexp_free, NULL);
	if (!general)
		return -1;
	context = regexp_match_context(general);
	data = context ? pcre2_match_data_create(nmatches, general) : NULL;
	pcre2_general_context_free(general);
	if (!data) {
		pcre2_match_context_free(context);
		git_error_set_oom();
		return -1;
	}

	if ((error = pcre2_match(*r, (const unsigned char *) string,
			strlen(string), 0, 0, data, context)) < 0)
		goto out;

	captures = (size_t) error;

	if (!captures || captures > nmatches)
		captures = nmatches;

	ovec = pcre2_get_ovector_pointer(data);

	for (i = 0; i < captures; i++) {
		matches[i].start = (ovec[i * 2] == PCRE2_UNSET) ? -1 : (ssize_t) ovec[i * 2];
		matches[i].end = (ovec[i * 2 + 1] == PCRE2_UNSET) ? -1 :  (ssize_t) ovec[i * 2 + 1];
	}

	for (i = captures; i < nmatches; i++)
		matches[i].start = matches[i].end = -1;

out:
	pcre2_match_data_free(data);
	pcre2_match_context_free(context);
	if (error < 0)
		return (error == PCRE2_ERROR_NOMATCH) ? GIT_ENOTFOUND : GIT_EINVALIDSPEC;
	return 0;
}

#elif defined(GIT_REGEX_REGCOMP) || defined(GIT_REGEX_REGCOMP_L)

#if defined(GIT_REGEX_REGCOMP_L)
# include <xlocale.h>
#endif

int git_regexp_compile(git_regexp *r, const char *pattern, int flags)
{
	int cflags = REG_EXTENDED, error;
	char errmsg[1024];

	if (flags & GIT_REGEXP_ICASE)
		cflags |= REG_ICASE;

# if defined(GIT_REGEX_REGCOMP)
	if ((error = regcomp(r, pattern, cflags)) != 0)
# else
	if ((error = regcomp_l(r, pattern, cflags, (locale_t) 0)) != 0)
# endif
	{
		regerror(error, r, errmsg, sizeof(errmsg));
		git_error_set_str(GIT_ERROR_REGEX, errmsg);
		return GIT_EINVALIDSPEC;
	}

	return 0;
}

void git_regexp_dispose(git_regexp *r)
{
	regfree(r);
}

int git_regexp_match(const git_regexp *r, const char *string)
{
	int error;
	if ((error = regexec(r, string, 0, NULL, 0)) != 0)
		return (error == REG_NOMATCH) ? GIT_ENOTFOUND : GIT_EINVALIDSPEC;
	return 0;
}

int git_regexp_search(const git_regexp *r, const char *string, size_t nmatches, git_regmatch *matches)
{
	regmatch_t static_m[3], *m;
	int error;
	size_t i;

	if (nmatches <= ARRAY_SIZE(static_m))
		m = static_m;
	else
		m = git__calloc(nmatches, sizeof(*m));

	if ((error = regexec(r, string, nmatches, m, 0)) != 0)
		goto out;

	for (i = 0; i < nmatches; i++) {
		matches[i].start = (m[i].rm_so < 0) ? -1 : m[i].rm_so;
		matches[i].end = (m[i].rm_eo < 0) ? -1 : m[i].rm_eo;
	}

out:
	if (nmatches > ARRAY_SIZE(static_m))
		git__free(m);
	if (error)
		return (error == REG_NOMATCH) ? GIT_ENOTFOUND : GIT_EINVALIDSPEC;
	return 0;
}

#endif
