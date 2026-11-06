# Git projects on Tab5OS

Git keeps a project's history, stages changes and exchanges them with a remote repository. It is available from Explorer and the terminal. A project can reside on the SD card or a connected USB drive.

## Open or create a project in Explorer

Open **Actions → Git: status and commands**, the Git icon or the right-click menu. The latest selection in either Explorer pane determines the target. With no selection, the target is the active pane's folder. A selected folder becomes the target; a selected file remains the target for file operations. Repository detection walks up to the repository root, so it also works from a subfolder.

Git occupies Explorer's right pane. The file list on the left remains usable, and the panel follows the chosen context after any running command finishes. **Back to preview · Esc** closes the pane without losing the browsed folder. The current folder and repository root are stored separately.

The **File** menu offers **Create Git folder** (the new subfolder's name) and **Clone Git repository** (an HTTPS address and the new subfolder's name). Cloning refuses an existing destination. To turn an existing ordinary folder into a project, select it and choose **Actions → Initialize this folder with Git**. Its contents remain in place, and initialization creates Git metadata in that folder.

Ordinary Explorer commands remain available: copy/cut/paste and multiple selection in **Edit**; rename, hide, delete, information and file tools in **Actions**; hidden files and refresh in **View**. Right-clicking a row targets that item and preserves a multiple selection when the item is already part of it. Right-clicking a pane's background targets its current folder.

Repository commands become available when the folder contains a recognized repository. Network commands appear when the upstream branch's remote is configured (or `origin` if no upstream is defined). Updating and pushing require a branch with at least one commit; these actions are hidden for an empty repository or a detached HEAD. The stop button appears during an operation.

| Action | Effect |
| --- | --- |
| Refresh | Reread the branch and changed files. |
| Stage file / this folder | Stage the targeted file or changes in the current folder, without including other subfolders of the repository. |
| Stage all | Explicitly stage all changes in the repository. |
| Create commit | Commit the repository's entire index using the entered message: a commit is not limited to the displayed file. |
| View differences | Show local or staged changes for the targeted file or current folder. An untracked file has no Git diff until it is added to the index. |
| History | Display the last 20 commits. |
| Fetch remote commits | Download new commits without changing working files. |
| Update without merging | Download changes, then fast-forward the branch if the working files are clean. |
| Push commits | Request confirmation, then send the current branch's commits to its configured upstream branch (or the same branch name on `origin` if no upstream is defined). |
| Configure | Set the author identity, the upstream branch's remote address (or `origin` if no upstream is defined) and the names of vault credentials. |
| Open terminal here | Open the terminal in the working folder retained by Explorer. |

**Stage** and **Commit** save locally. **Push** publishes commits to the server. **Fetch** does not modify local files: use **Update** to apply received commits. Output lists changed references and their old and new IDs; pushing also displays the destination branch. A fetch with no new references or an already up-to-date branch is reported explicitly. A server rejection remains an error even if transport completed successfully.

The local branch and its upstream branch may have different names. Commands now use `branch.<name>.remote`, `branch.<name>.merge` and the remote's fetch references. The push confirmation displays the exact destination. The **To push** and **To receive (last checked)** counters compare local state with the most recently fetched references; Refresh alone does not contact the server. Remote-address configuration also targets that upstream remote.

The author's name and email address must be set before the first commit. If either is missing, Commit opens Configure with instructions; enter the name and email, then run Commit again. These settings are saved in the project.

Actions with nothing to do are disabled: **Stage** when the selected file (or folder, with no selection) has no more changes to stage; **Create commit** when nothing is staged or a conflict remains; **View differences** when the project is clean; and the output trash icon when output is empty. **Update without merging** requires a project with no local changes. Changes and conflicts on other pages are also taken into account. During an operation, commands that would modify the repository remain disabled.

The list shows its range and the actual number of changed files, for example `129–256 / 300`. It scrolls within a page of 128 files; page arrows provide access to the rest. Pagination does not limit the total number of accessible files. Selection and page are preserved after staging a file. Row indicators are: `+` for a staged change, `M` for a local change, `?` for a new file, `D` for a deletion and `!` for a conflict reported by the repository.

The pane's output area contains the command result and its final status. For very long output, the last lines are retained and the interface indicates this. The text remains selectable and copyable. The output trash icon, **Ctrl+L**, or **right-click → Clear output** clears only this text; no file, commit or process is deleted.

**New** creates a local repository. **Clone** downloads an HTTPS repository into a new subfolder. **Configure** separates author identity from server access: the name and email identify the author of commits; **Vault username** and **Vault token** select authentication credentials. A public repository can be cloned without these fields. For a private repository, use **Unlock vault** in the Git workflow, enter the PIN, then select the two secrets already saved in the vault. Create secrets in the vault settings; unlocking from Git allows their use without leaving Explorer. A credentials file remains an advanced option. No token is placed in the repository URL.

## Keyboard and text input

| Shortcut | Action |
| --- | --- |
| Tab / Shift+Tab | Move focus between controls and fields. |
| Up / Down arrow | Browse files, including beyond the currently visible rows. |
| Alt+Left / Alt+Right | Previous / next page of files. |
| F5 | Refresh the project. |
| Ctrl+L | Clear Git output when outside an input dialog. |
| Ctrl+. | Request cancellation of the current operation. |
| Enter | Confirm a single-line field; in Clone, move from the address to the folder, then confirm. |
| Ctrl+Enter | Submit the form from any field. |
| Escape | Close an open dropdown first, then the input dialog or project. |
| Ctrl+A/C/X/V/Z/Y | Select, copy, cut, paste, undo or redo in an editable field. |

Fields also provide the OS editing icons. When a dialog opens, the first required field receives focus. When the touch keyboard appears, the form scrolls and the confirmation control remains above the keyboard.

## Terminal workflow

Paths such as `sd:/…` refer to the SD card and `usb:/…` to the USB drive. Relative paths start from the current folder. Put names containing spaces in quotes.

```text
cd sd:/Projects
git init demo
cd demo
git config user.name "Your name"
git config user.email "you@example.org"
git status
git add -- programme.py
git diff --cached
git commit -m "First program"
git log -n 20
```

To operate from another folder, place `-C` before the command name:

```text
git -C usb:/Projects/demo status
```

To clone a public repository into a new folder:

```text
git clone https://example.org/equipe/projet.git projet
```

By default, cloning retrieves the latest state with a depth of 1. To request the complete history, within the device's limits:

```text
git clone --depth 0 https://example.org/equipe/projet.git projet-complet
```

To configure a remote and exchange changes:

```text
git remote set-url origin https://example.org/equipe/projet.git
git remote -v
git fetch
git pull --ff-only
git push
```

`remote set-url origin` also creates `origin` if it does not exist. `fetch` retrieves remote objects and references; `pull --ff-only` also updates local files; `push` publishes local commits. A file archive alone is not a replacement for a Git repository: copying the entire folder, including `.git`, preserves its local history.

## Private repositories and authentication

Git transfers use HTTPS. The **Clone** form lets you choose **Vault username** and **Vault token**, after unlocking with a PIN if necessary. Both names are saved in the new repository for future transfers. In an already open repository, set these choices through **Configure**.

As an advanced option, a text file may contain two lines: the username, then the token supplied by the hosting service. This file contains a secret: keep it outside the project folder so it is not added to a commit. For example, store it at `sd:/Secrets/git-host.txt`. In an already open repository, enter its path under **Configure → Credentials file (advanced)**. To use this file for the initial clone, use the terminal's `--credential-file` option below.

The terminal receives only the file path, never the token itself:

```text
git clone --credential-file sd:/Secrets/git-host.txt https://example.org/equipe/prive.git prive
```

Cloning saves the credentials file path in the new repository for subsequent transfers. Only the path is stored in the configuration, not the secret contents.

The file must not contain any additional lines. A failed read produces an explicit error. Credentials embedded in the URL are rejected. Certificates and the server name are verified; redirects are rejected. Connections use the OS network supervisor.

## Cancellation and recovery

The stop button and `Ctrl+.` request cooperative cancellation. A network read already in progress may last until its timeout. Closing the project during a command also requests cancellation. The context is retained until the worker's final access; FreeRTOS then releases its stack.

A clone first uses a temporary folder with the `.git-import` suffix, renamed on completion. After an interrupted clone, this folder may remain and a new attempt using the same name is rejected. Inspect the folder on a computer or choose another destination to start again; the recovery described below applies to a `pull` update, not an incomplete clone.

File updates performed by `pull` are journaled. After an interruption, diagnostics may request:

```text
git recover --continue
```

Run this command inside the affected repository, then check `git status`. File recovery is not performed automatically at startup. While recovery is required, modifying commands are blocked; status, history, differences and configuration remain readable.

The `.git/tab5-pull.pending` marker stores the old commit, target commit and reference to advance. The reference remains at the old commit until file updates, synchronization and the index are complete. Recovery refuses to overwrite a divergent file, a modified index or a missing modified file; it also refuses if the branch has changed in the meantime. Untracked files and unrelated changes are preserved.

A corrupt journal or partially written file requires inspection on a computer. Keep the files and journal to investigate the diagnostic; do not delete the marker to bypass the block. Synchronizing writes reduces risk but does not guarantee FAT filesystem integrity during a power failure.

## Scope and limits of this version

The interface runs operations outside the display loop and reuses its visible rows. Reading status, staging, committing, configuring or fetching no longer rebuilds Explorer's list. File creation and updates notify the OS when finished, including from the terminal, so the list and repository detection are invalidated at the appropriate time.

Content comparisons no longer rely only on file size and date: Status, Diff and Stage detect two saves of the same size within FAT timestamp resolution. The index's timestamp cache is invalidated in memory at the start of these operations, then contents are compared. Read-only queries do not rewrite the index on disk, and there is no periodic Git scan. The status view is bounded to 128 entries per page, with a 32 KiB report. The Git engine is serialized: only one Git operation runs at a time, from either the interface or the terminal.

The Git engine's allocator is limited to 8 MiB of PSRAM, with no SRAM fallback for those allocations. Its cache is limited to 256 KiB; pack read windows use 128 KiB each, 256 KiB in total and at most four files. A pack is limited to 32,768 objects and an incoming transfer to 128 MiB. Text diffs handle files of at most 2 MiB. A repository exceeding these limits may be rejected: a shallow clone reduces the downloaded history, but not the size of its current state.

`pull` requires a clean working tree to fast-forward the current branch. An already up-to-date state or a local branch that is only ahead is reported explicitly; divergence or an upstream branch deleted on the server is rejected without moving the local branch. Merges, rebase, branch switching, submodules, LFS, repositories with alternates and linked worktrees are unsupported. Volume roots cannot become repositories. This engine does not support Git over SSH; the terminal's SSH command is separate.
