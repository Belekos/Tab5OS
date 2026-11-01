#pragma once
#include "Tab5Git.h"
#include "src/tab5git/include/git2.h"

namespace Tab5GitPull {
// A pending or unreadable intent blocks new mutations. Reading status/log/diff
// remains possible. Recovery is always explicitly requested by the user.
bool pending(git_repository* repo);
int start(git_repository* repo, git_reference* previous, git_commit* target,
          const Tab5Git::Cancel& cancel = {});
int resume(git_repository* repo, const Tab5Git::Cancel& cancel = {});
// Non-null for our actionable translated diagnostics, null for libgit2 errors.
const char* diagnostic(int result);
#ifdef TAB5_GIT_PULL_TEST
// Process-exit fault injection; never enabled in the firmware.
using Checkpoint = void (*)(const char* stage, size_t completed);
void checkpoint(Checkpoint callback);
#endif
}
