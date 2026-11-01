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

#include "git2_util.h"

#if !defined(GIT_THREADS)

#define TLSDATA_MAX 16

typedef struct {
	int in_use;
	void *value;
	void (GIT_SYSTEM_CALL *destroy_fn)(void *);
} tlsdata_value;

static tlsdata_value tlsdata_values[TLSDATA_MAX];

int git_tlsdata_init(git_tlsdata_key *key, void (GIT_SYSTEM_CALL *destroy_fn)(void *))
{
	int i;
	if (!key)
		return -1;
	/* Init/shutdown runs once per embedded command. Reuse disposed keys,
	 * including holes, instead of exhausting the table after 16 commands. */
	for (i = 0; i < TLSDATA_MAX; ++i) {
		if (tlsdata_values[i].in_use)
			continue;
		tlsdata_values[i].in_use = 1;
		tlsdata_values[i].value = NULL;
		tlsdata_values[i].destroy_fn = destroy_fn;
		*key = i;
		return 0;
	}
	return -1;
}

int git_tlsdata_set(git_tlsdata_key key, void *value)
{
	if (key < 0 || key >= TLSDATA_MAX || !tlsdata_values[key].in_use)
		return -1;

	tlsdata_values[key].value = value;
	return 0;
}

void *git_tlsdata_get(git_tlsdata_key key)
{
	if (key < 0 || key >= TLSDATA_MAX || !tlsdata_values[key].in_use)
		return NULL;

	return tlsdata_values[key].value;
}

int git_tlsdata_dispose(git_tlsdata_key key)
{
	void *value;
	void (*destroy_fn)(void *) = NULL;

	if (key < 0 || key >= TLSDATA_MAX || !tlsdata_values[key].in_use)
		return -1;

	value = tlsdata_values[key].value;
	destroy_fn = tlsdata_values[key].destroy_fn;

	tlsdata_values[key].value = NULL;
	tlsdata_values[key].destroy_fn = NULL;
	tlsdata_values[key].in_use = 0;

	if (value && destroy_fn)
		destroy_fn(value);

	return 0;
}

#elif defined(GIT_WIN32)

int git_tlsdata_init(git_tlsdata_key *key, void (GIT_SYSTEM_CALL *destroy_fn)(void *))
{
	DWORD fls_index = FlsAlloc(destroy_fn);

	if (fls_index == FLS_OUT_OF_INDEXES)
		return -1;

	*key = fls_index;
	return 0;
}

int git_tlsdata_set(git_tlsdata_key key, void *value)
{
	if (!FlsSetValue(key, value))
		return -1;

	return 0;
}

void *git_tlsdata_get(git_tlsdata_key key)
{
	return FlsGetValue(key);
}

int git_tlsdata_dispose(git_tlsdata_key key)
{
	if (!FlsFree(key))
		return -1;

	return 0;
}

#elif defined(_POSIX_THREADS)

int git_tlsdata_init(git_tlsdata_key *key, void (GIT_SYSTEM_CALL *destroy_fn)(void *))
{
	if (pthread_key_create(key, destroy_fn) != 0)
		return -1;

	return 0;
}

int git_tlsdata_set(git_tlsdata_key key, void *value)
{
	if (pthread_setspecific(key, value) != 0)
		return -1;

	return 0;
}

void *git_tlsdata_get(git_tlsdata_key key)
{
	return pthread_getspecific(key);
}

int git_tlsdata_dispose(git_tlsdata_key key)
{
	if (pthread_key_delete(key) != 0)
		return -1;

	return 0;
}

#else
# error unknown threading model
#endif
