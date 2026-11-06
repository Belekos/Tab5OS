# Shared environment and vault

The vault stores named, encrypted entries. First access requires setting a PIN; changing that PIN requires the current PIN. An entry becomes a protected variable after unlocking. Its value is read again on each request; locking the vault blocks new reads, including from a terminal that is already open. Deleting the entry invalidates its reference: it never falls back to an old public value with the same name.

The public environment exists only in RAM for the current boot session and is shared between applications. Closing the terminal or ending a Python script does not destroy it. Local `set NAME value` variables remain specific to the terminal; `export NAME=value` creates a shared public value. Protected references retain only the entry name, never a copy of the password. There are at most 64 shared variables, with ASCII names up to 48 characters and values up to 512 bytes.

## Shell

```sh
export LANG=en-GB
export --secret API_TOKEN MY_TOKEN
env
# env displays names only, without values.
# Normal expansion; quote values that may contain spaces:
command "$API_TOKEN"
unset API_TOKEN
```

`export --secret NAME [ENTRY]` creates a reference without reading the secret. Without a second name, the entry has the same name as the variable. `unset` removes the RAM binding without deleting the vault entry; the next unlock imports the remaining entry names again. A public export cannot overwrite a protected reference: first remove that binding explicitly.

Assignments and `set` / `export` commands are neither echoed as commands nor added to terminal history. History never contains automatic expansions. A program, or an explicitly requested `echo "$API_TOKEN"`, can nevertheless display the value. The system does not claim to isolate programs: a program given a secret can copy, retain, transmit or display it. Locking does not revoke a copy already obtained by a program. Scripts on the SD card should not contain plaintext secrets.

Explorer launches `.sh` files in the native terminal with their parent folder as the working directory. Paths containing spaces or apostrophes are protected; SD and USB storage are supported. Scripts use the Tab5 terminal grammar, with a 32 KiB limit and sequential command execution.

## MicroPython

```python
import tab5
tab5.setenv("LANG", "en-GB")       # public value in RAM
tab5.bindenv("API_TOKEN", "MY_TOKEN")
token = tab5.env("API_TOKEN")     # OSError if the vault is locked or entry is absent
names = tab5.env_names()          # names only
tab5.unsetenv("API_TOKEN")
```

`tab5.env("UNKNOWN", default_value)` uses the default only if no binding exists. It never hides a protected reference's locked-vault error. This interface belongs to the supplied `tab5` module; it does not modify `os.getenv` or the libc environment.

## Native and ELF applications

Native code includes `Tab5Environment.h`. `tab5_env_get(name, buffer, capacity)` copies the value and returns its length, or a negative errno; it clears the supplied buffer on failure. `tab5_env_set`, `tab5_env_bind` and `tab5_env_unset` follow the same rules as the shell. Values are copied; no internal pointer is exposed.

ELF ABIs 3, 4 and 5 use their existing `hardware` callback. The format is the same as MicroPython's:

| Operation | a | Input | Output |
|---|---:|---|---|
| 107, read | 0 | `NAME\0` | UTF-8 value, length excluding NUL |
| 108, public | 0 | `NAME\0VALUE\0` | 0 or negative errno |
| 108, reference | 1 | `NAME\0ENTRY\0` | 0 or negative errno |
| 109, remove | 0 | `NAME\0` | 0 or negative errno |
| 109, names | 1 | None | Names separated by `\n` |

```c
char token[513];
int n = api3->hardware(107, 0, 0, 0, "API_TOKEN", 10,
                      token, sizeof(token));
/* Use only if n >= 0, then clear token after use. */
```

ABIs 1 and 2 remain compatible but do not expose this broker. ELF applications are trusted code sharing system memory; the vault is not a sandbox.

## Git

Create two vault entries, for example `GIT_USER` and `GIT_TOKEN`, then choose them in **Git identity and access**, or in the clone form's two selectors. **None** removes a reference. To use an old credentials file again, remove both vault references. That file remains compatible; its protection is that of the existing file.

```sh
git config tab5.usernameSecret GIT_USER
git config tab5.tokenSecret GIT_TOKEN
git fetch
# Clone a private repository:
git clone --username-secret GIT_USER --token-secret GIT_TOKEN https://example.org/projet.git projet
# Remove references:
git config --unset tab5.usernameSecret
git config --unset tab5.tokenSecret
```

Only names are saved in `.git/config`. Credentials are resolved when the server requests them, only for the expected HTTPS origin. An incomplete, deleted or locked reference causes authentication to fail without falling back to the credentials file. Values are placed neither in the URL nor in Git status output. An already authenticated transfer may continue after locking: data already given to the connection is not recalled.

## Loading credentials

Credentials become available only after their store loads successfully. A read failure is remembered for the session; interface calls do not trigger a read/decrypt loop. **Retry** starts a new explicit attempt. Secret variables remain unavailable if the vault or credential store is not open. See [SECURITE.md](SECURITE.md).
