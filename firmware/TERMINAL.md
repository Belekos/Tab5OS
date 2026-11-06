# Terminal

`help` groups commands by category; `help COMMAND` and `COMMAND --help` describe their parameters and provide an example. Network commands from the previous version remain available.

## First examples

```sh
pwd
ls -al /Documents
cd /USB
df
ping -c 3 -W 0.5 192.168.1.1
dns example.org
tcp -w 2 example.org 443
ports -w 0.35 192.168.1.1 22,80,443
headers -L https://example.org
wget -L -T 300 -O usb:/Documents/page.html https://example.org
curl -L -o sd:/Documents/page.html https://example.org
sha256 /USB/firmware.bin
```

Replace the hosts and paths with the ones you want to use. The destination directory must already exist. A download preserves any previous file until the transfer succeeds. Add `--overwrite` to replace an existing file; an error or cancellation removes the temporary file. HTTPS checks both the certificate and the hostname; redirects from HTTPS to HTTP are rejected.

## Paths and editing

| Path syntax | Destination |
|---|---|
| `/Documents/notes.txt` or `sd:/Documents/notes.txt` | SD card |
| `/USB/Documents/notes.txt` or `usb:/Documents/notes.txt` | Mounted USB storage |
| `notes.txt` | Current directory |
| `..` | Parent directory |

Paths containing spaces must be quoted. Storage roots cannot be deleted. The terminal offers the same editing actions as other text fields: **Ctrl+A/C/X/V/Z/Y**, and **Ctrl+Shift+Z** to redo. The corresponding buttons use icons and adapt to the active field. Output can be selected but remains read-only.

**Up/Down** browses history in the command field; **Tab** completes input. **Ctrl+C** copies a selection, or interrupts the operation when nothing is selected. **Ctrl+.** also interrupts. **Ctrl+Enter** executes the command. The Meta key can replace Ctrl for editing.

**Ctrl+L**, the trash button, or **right-click → Clear output** clears the stored output. The current directory, variables and command history remain available. Clearing output does not stop a running command: new lines continue to appear, without restoring old lines on refresh. The `clear` command is also available.

## Pipes, input and substitutions

```sh
ls /Documents | grep .txt
cat journal.txt | grep -i error | tail -n 20
seq 1 100 | head -n 5 | wc -l
grep -in error < journal.txt > erreurs.txt
echo "The current directory is $(pwd)"
set directory "$(dirname /Documents/notes.txt)"
echo `pwd`
```

`|` passes one command's output to the next command's input. `cat`, `head`, `tail`, `grep` and `wc` read this input when no file is specified; `-` explicitly refers to it in a list of files. Without a pipe or redirection, this input is empty: these commands do not wait for interactive input. `< FILE` replaces the input; `>` and `>>` retain their overwrite and append behavior. Error messages remain in the terminal and are not sent through the pipe.

Commands in a pipeline run sequentially. Intermediate output is stored in PSRAM, without writing a temporary file to the SD card. The `$?` status is that of the last command, as with a conventional shell pipeline. Variable or directory changes made in a pipeline remain local to the command that makes them.

`$(COMMAND)` and backticks capture output and remove trailing newlines. Double quotes preserve spaces and internal newlines; without quotes, the output is split into arguments. Single quotes disable this interpretation. Substitutions may contain pipes and other substitutions. Their directory and variables do not replace those of the terminal. An assignment on its own, such as `result=$(false)`, retains the substitution's exit status.

Explicit limits: 16 commands per pipeline; 256 KiB between commands; 4 substitution levels and 8 KiB per substitution; 512 arguments and 32 KiB of expanded arguments. The combined nesting of script calls, loops and substitutions is also limited to six simultaneous execution blocks to protect the task stack. Exceeding a limit produces an error. Pipes are intended for finite streams; infinite producers, background tasks (`&`), command groups (`(COMMANDS)`) and separate error redirection (`2>`) are not implemented. `Ctrl+C` with no selection and the Stop button cancel execution.

## Responsiveness and memory

The cancellation check no longer suspends the task for every file or text fragment: it checks cancellation immediately and yields the CPU every five milliseconds. Output uses a 32 KiB circular buffer, avoiding moving all the text for every new line when the buffer is full. While a command runs, the display refreshes at most twice per second; the final result appears on the next UI service pass. An active selection remains intact.

Output, its display snapshot, variable values, command history, pipes, captured substitutions and the content retained by `tail` explicitly use PSRAM. Small structures, working strings, arguments and the task stack still use the system allocator. The stack is freed only when the task has actually terminated; the context is destroyed after that signal, never while a command is running. Temporary data is released when it goes out of scope; closing the terminal also releases history and variables.

`memory` separately reports free internal RAM, its largest free block, free PSRAM and its largest free block. Use it to check the tablet's actual memory state after running several commands.

## Complete reference

In the syntax below, square brackets indicate an optional parameter and `...` indicates multiple arguments. Do not type these markers literally. In the syntax for `test` and `ip`, `|` separates possible choices: do not type this character when selecting an argument.

### `help`

```text
help [COMMAND]
```

Lists commands; add a command name to see its parameters and an example.

Example:

```sh
help wget
```

### `pwd`

```text
pwd
```

Displays the current directory.

Example:

```sh
pwd
```

### `cd`

```text
cd [DIR]
```

Changes directory. With no parameter, switches to the SD root. `..` denotes the parent directory; `/USB` denotes USB storage.

Example:

```sh
cd /USB/Documents
```

### `ls`

```text
ls [-a] [-l] [DIR]
```

Lists the current or specified directory. `-a` includes hidden files; `-l` displays the type and size.

Example:

```sh
ls -al /Documents
```

### `tree`

```text
tree [DIR]
```

Lists all paths in a directory and its subdirectories, up to 16 levels deep.

Example:

```sh
tree /Documents
```

### `stat`

```text
stat FILE...
```

Displays the type, size in bytes and modification date of each path.

Example:

```sh
stat /Documents/notes.txt
```

### `du`

```text
du [DIR]
```

Calculates the total file size in a directory, including subdirectories and hidden files.

Example:

```sh
du /Documents
```

### `cat`

```text
cat [FILE...]
```

Displays text files in the specified order. With no file, reads input from a pipe or `<` redirection; `-` refers to that input.

Example:

```sh
cat notes.txt
```

### `head`

```text
head [-n COUNT] [FILE...]
```

Displays the first lines. `-n`: 1 to 500 lines; default: 10. With no file, reads input from a pipe or `<` redirection.

Example:

```sh
head -n 20 notes.txt
```

### `tail`

```text
tail [-n COUNT] [FILE...]
```

Displays the last lines. `-n`: 1 to 500 lines; default: 10. Retains at most 64 KiB. With no file, reads input from a pipe or `<` redirection.

Example:

```sh
tail -n 30 journal.txt
```

### `wc`

```text
wc [-lwc] [FILE...]
```

Counts newlines, words and bytes. `-l`, `-w` and `-c` select the counts to display. With no file, reads input from a pipe or `<` redirection.

Example:

```sh
wc notes.txt
```

### `grep`

```text
grep [-i] [-n] TEXT [FILE...]
```

Searches for literal text. `-i` ignores ASCII case; `-n` displays line numbers. No match returns exit code 1. With no file, reads input from a pipe or `<` redirection.

Example:

```sh
grep -in error journal.txt
```

### `find`

```text
find [DIR] [-name PATTERN]
```

Searches recursively, up to 16 levels deep. `-name` filters by filename; enclose patterns in single quotes.

Example:

```sh
find /Documents -name '*.txt'
```

### `hexdump`

```text
hexdump [-n BYTES] [-s OFFSET] FILE
```

Displays hexadecimal bytes and text. `-n`: 1 to 65536 bytes; default: 256. `-s`: starting offset in bytes.

Example:

```sh
hexdump -n 64 -s 16 image.bin
```

### `cmp`

```text
cmp FILE1 FILE2
```

Compares two files, reports the first differing byte and returns 1 if they differ.

Example:

```sh
cmp original.bin copie.bin
```

### `mkdir`

```text
mkdir [-p] DIR...
```

Creates directories. `-p` also creates missing parent directories and accepts a directory that already exists.

Example:

```sh
mkdir -p /Documents/Projet
```

### `rmdir`

```text
rmdir DIR...
```

Removes only empty directories. Storage roots remain protected.

Example:

```sh
rmdir /Documents/Vide
```

### `touch`

```text
touch FILE...
```

Creates an empty file if it does not exist; preserves the contents of an existing file.

Example:

```sh
touch notes.txt
```

### `cp`

```text
cp SOURCE DEST
```

Copies a file to another file or directory. The previous destination remains intact if copying fails.

Example:

```sh
cp notes.txt /USB/Documents
```

### `mv`

```text
mv SOURCE DEST
```

Moves or renames a file or directory on the same storage device. Refuses to overwrite an existing destination.

Example:

```sh
mv brouillon.txt notes.txt
```

### `rm`

```text
rm [-r] PATH...
```

Deletes the specified paths immediately. `-r` includes directory contents. Storage roots remain protected.

Example:

```sh
rm brouillon.txt
```

### `basename`

```text
basename PATH
```

Displays the last component of a path.

Example:

```sh
basename /Documents/notes.txt
```

### `dirname`

```text
dirname PATH
```

Displays the parent directory of a path.

Example:

```sh
dirname /Documents/notes.txt
```

### `echo`

```text
echo [-n] TEXT...
```

Displays arguments separated by spaces. `-n` omits the trailing newline.

Example:

```sh
echo 'Hello' > notes.txt
```

### `printf`

```text
printf FORMAT [TEXT...]
```

Prints formatted text: `%s` inserts an argument, `%%` a percent sign, `\n` a newline and `\t` a tab.

Example:

```sh
printf '%s\n' Hello
```

### `seq`

```text
seq [START] END
```

Displays a sequence of increasing integers, one per line. Default starting value: 1; maximum: 10000 values.

Example:

```sh
seq 3 8
```

### `clear`

```text
clear
```

Clears the terminal display; preserves history and variables.

Example:

```sh
clear
```

### `set`

```text
set NAME VALUE
```

Sets a variable; `name=value` is also accepted. Use `$name` or `${name}` to read it. Maximum: 32 variables.

Example:

```sh
set directory /Documents
```

### `unset`

```text
unset NAME
```

Removes a variable.

Example:

```sh
unset directory
```

### `env`

```text
env
```

Displays variables and `PWD`, the current directory.

Example:

```sh
env
```

### `test`

```text
test -e PATH | -f PATH | -d PATH | TEXT = TEXT | TEXT != TEXT
```

Tests whether a path exists (`-e`), is a file (`-f`), is a directory (`-d`), or compares two strings. Returns 0 for true and 1 for false.

Example:

```sh
test -f notes.txt && cat notes.txt
```

### `true`

```text
true
```

Returns exit code 0; useful in script conditions.

Example:

```sh
true && echo yes
```

### `false`

```text
false
```

Returns exit code 1; useful in script conditions.

Example:

```sh
false || echo no
```

### `source`

```text
source FILE.sh [ARGS...]
```

Runs a script in the current terminal. `$0` to `$9` contain its path and arguments. Maximum script size: 32 KiB.

Example:

```sh
source /Scripts/sauvegarde.sh /Documents
```

### `sh`

```text
sh FILE.sh [ARGS...]
```

Runs a script in the current terminal, like `source`. Directory and variable changes persist.

Example:

```sh
sh /Scripts/sauvegarde.sh /Documents
```

### `for`

```text
for NAME in VALUES; do COMMANDS; done
```

Repeats commands for each value. Maximum: 4 levels of scripts or loops and 10000 commands per invocation.

Example:

```sh
for f in /Documents/*.txt; do wc "$f"; done
```

### `date`

```text
date
```

Displays the OS date and time.

Example:

```sh
date
```

### `df`

```text
df [DIR]
```

Displays free space and capacity for the storage device containing the specified directory; defaults to the current directory.

Example:

```sh
df /USB
```

### `memory`

```text
memory
```

Separately displays free memory and the largest allocatable block in internal SRAM and PSRAM.

Example:

```sh
memory
```

### `uptime`

```text
uptime
```

Displays the number of seconds since startup.

Example:

```sh
uptime
```

### `sleep`

```text
sleep SECONDS
```

Waits for 0 to 3600 seconds; use a decimal point for fractions. Ctrl+C interrupts the wait.

Example:

```sh
sleep 0.5
```

### `sha256`

```text
sha256 FILE...
```

Calculates the SHA-256 checksum of each file on SD or USB storage.

Example:

```sh
sha256 /USB/firmware.bin
```

### `wifi`

```text
wifi
```

Displays the network name and IPv4 address, or the disconnected state.

Example:

```sh
wifi
```

### `netinfo`

```text
netinfo
```

Displays the Wi-Fi network, IPv4 address, netmask, gateway, DNS and signal strength. Aliases: `ifconfig`, `ip addr`, `ip route`.

Example:

```sh
netinfo
```

### `ifconfig`

```text
ifconfig
```

Alias for `netinfo`: displays the current network configuration.

Example:

```sh
ifconfig
```

### `ip`

```text
ip [addr|route]
```

Displays the IPv4 address, netmask, gateway and DNS. Route modification is not available.

Example:

```sh
ip route
```

### `dns`

```text
dns HOST
```

Resolves a DNS name to IPv4, with a maximum timeout of 5 seconds. Alias: `nslookup`.

Example:

```sh
dns example.org
```

### `nslookup`

```text
nslookup HOST
```

Alias for `dns`: resolves a DNS name to IPv4.

Example:

```sh
nslookup example.org
```

### `ping`

```text
ping [-c COUNT] [-W SECONDS] HOST
```

Tests ICMP responses. `-c`: 1 to 100 requests; default: 4. `-W`: timeout per request; default: 1 second.

Example:

```sh
ping -c 3 -W 0.5 192.168.1.1
```

### `tcp`

```text
tcp [-w SECONDS] HOST PORT
```

Tests whether a TCP port accepts connections, without sending data. Port: 1 to 65535. `-w`: connection timeout; default: 1.5 seconds.

Example:

```sh
tcp -w 2 example.org 443
```

### `nc`

```text
nc [-z] [-w SECONDS] HOST PORT
```

Alias for `tcp`. `-z` tests the connection without sending data. This client does not create an interactive session.

Example:

```sh
nc -z -w 2 192.168.1.1 80
```

### `ports`

```text
ports [-w SECONDS] HOST PORTS
```

Tests ports on a device. Specify a list such as `22,80,443` or a range such as `8000-8010`; maximum: 1024 ports. `-w`: timeout per port; default: 1.5 seconds.

Example:

```sh
ports -w 0.35 192.168.1.1 22,80,443,8000-8010
```

### `headers`

```text
headers [-L] [-T SECONDS] URL
```

Displays HTTP status, the verified HTTPS certificate and headers useful for auditing. `-L` follows up to 5 redirects; `-T` limits the total duration to 1–3600 seconds, with a default of 120.

Example:

```sh
headers -L https://example.org
```

### `wget`

```text
wget [-O FILE] [-L] [-T SECONDS] [--overwrite] URL
```

Downloads to SD or USB storage. `-O` selects the file; otherwise, its name comes from the URL. `-L` follows redirects; `-T` limits the total duration to 1–3600 seconds (default: 120). `--overwrite` replaces an existing file after a successful transfer.

Example:

```sh
wget -L -O usb:/Documents/page.html https://example.org
```

### `curl`

```text
curl [-I] [-o FILE|-O] [-L] [-T SECONDS] [--overwrite] URL
```

Displays an HTTP response (up to 32 KiB), or saves it with `-o FILE` or `-O` (filename from the URL). `-I` requests headers; `-L` follows redirects; `-T` limits the total duration to 1–3600 seconds, with a default of 120. HTTPS checks both the certificate and the hostname.

Example:

```sh
curl -L -o sd:/Documents/page.html https://example.org
```

## Scripts and exit codes

`;` separates commands, `&&` continues if the previous command succeeds, and `||` continues if it fails. `>` overwrites a file and `>>` appends text. Variables are accessed with `$name`, `${name}`, `$PWD` and `$?`. Script arguments are available as `$0` to `$9`.

`sh` and `source` use the same interpreter as the terminal: outside a pipe or substitution, directory and variable changes persist. Inside a pipe or substitution, these changes remain local. Pipes and substitutions are described above; background tasks (`&`) remain unavailable.

| Code | Meaning |
|---|---|
| 0 | Command succeeded or condition is true |
| 1 | Operational failure, false condition or no result, depending on the command |
| 2 | Invalid parameters or syntax; read the displayed explanation and syntax |
| 130 | Operation interrupted |

An open TCP port means that a connection was accepted; it does not prove that the service is secure or working correctly. A timeout may be caused by filtering or an unreachable device. DNS and network tests use IPv4; supply internationalized domain names in their DNS/Punycode form.

The **Pentest** application offers auditing operations and an exportable report in a dedicated interface. Network commands are described in [RESEAU_DIAGNOSTIC.md](RESEAU_DIAGNOSTIC.md).

## SSH, SFTP and tasks

`help ssh` describes remote commands, explicit trust in the server key, and key or password files. `help sftp` describes directory listings and file transfers without implicit overwriting. Complete examples, key formats, timeouts and limits are in [SSH.md](SSH.md). This version runs remote commands and their pipelines; it does not yet provide an interactive VT/PTY terminal.

`tasks` displays an actual snapshot of FreeRTOS tasks: identifier, known OS name, state, priority, minimum free stack in bytes, and the SDK runtime counter. The counter is a raw value, not a CPU percentage. `memory` complements this snapshot with memory resource information.

## Serial port

`serial` displays the status, `serial mode data` enables data exchange, and `serial send "Hello"` sends text. `serial write` preserves bytes and accepts input from a pipe or file; `serial read` and `serial listen` receive data. Settings are shared with Settings → System → Serial. See [SERIE.md](SERIE.md) for ports, timeouts, line endings and the complete command reference.
