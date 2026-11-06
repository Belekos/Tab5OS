# SSH and SFTP

`ssh` executes a command on an SSH server and returns its output, errors and exit status. `sftp` transfers a file or lists a remote directory. Connections use the OS network supervisor and close with **Ctrl+C**, the terminal's stop button, closing the terminal or disabling the network.

A remote command is a single operation: this version does not provide an interactive PTY/VT100 terminal. Use commands without full-screen interfaces, or scripts on the server. For example, `uname`, `git status`, a backup script or `systemctl status --no-pager` are suitable; `vim`, interactive `top` and a remote password prompt require another terminal.

## Establish server trust

1. On the server, or through an already trusted connection, obtain its host-key fingerprint. With OpenSSH, for example: `ssh-keygen -lf /etc/ssh/ssh_host_ecdsa_key.pub -E sha256`, or use the RSA key used by that server.
2. On Tab5OS, run `ssh keyscan serveur.example`. This command displays the received fingerprint without sending credentials; it does not prove the server's identity.
3. Compare the **entire** fingerprint with the one obtained independently, then record the exact value:

```sh
ssh trust serveur.example SHA256:EMPREINTE_VERIFIEE
```

`SHA256:EMPREINTE_VERIFIEE` is a placeholder to replace with your verified fingerprint, not a usable fingerprint. A real fingerprint contains 43 base64 characters after `SHA256:`.

Fingerprints are stored in the tablet's configuration memory, independently of the SD card, for up to 32 host/port pairs. The port is part of the identity: use `-p 2222` in `keyscan`, `trust` and connection commands when needed. If the key changes, authentication is blocked. After checking the cause on the server, `ssh forget serveur.example` removes only the old entry. A `trust` command never silently overwrites a different fingerprint.

## Authentication

The recommended method is an **RSA or ECDSA private key in PEM format**. Ed25519 keys and the `OPENSSH PRIVATE KEY` container are not supported by the supplied mbedTLS profile. A compatible ECDSA key can be created on a PC with:

```sh
ssh-keygen -t ecdsa -b 256 -m PEM -f id_tab5
```

Install the public key on the server, then place the private key and its companion public-key file `id_tab5.pub` in the same directory on the tablet. This companion file in OpenSSH format is required for ECDSA keys; RSA PEM keys can also be used alone. Example:

```sh
ssh -i sd:/Keys/id_tab5 user@serveur.example 'uname -a'
ssh -i usb:/Keys/id_tab5 user@serveur.example 'git -C /home/user/projet status --short'
```

`--passphrase-file FILE` supplies the encryption passphrase for a PEM key. For a server that permits passwords, `--password-file FILE` replaces `-i KEY`:

```sh
ssh --password-file sd:/Keys/server.secret user@serveur.example 'uname -a'
```

The secret file's contents are not included in the command line, history or display; its buffer is wiped before being freed. Trailing CR/LF line endings are removed. Limits: 16 KiB for a private key, 1 KiB for a secret. No `--password MOT_DE_PASSE` argument is accepted.

These files remain readable on their storage device. SSH protects their transfer over the network, not their storage at rest. A passphrase file placed beside an encrypted key does not offer equivalent protection to a passphrase entered and kept separately. This version has neither a secret-entry prompt nor automatic vault integration.

## Pipes, timeouts and results

```sh
cat sd:/Documents/notes.txt | ssh -i sd:/Keys/id_tab5 user@serveur.example 'cat > /home/user/notes.txt'
ssh -i sd:/Keys/id_tab5 user@serveur.example 'printf "resultat\n"' | grep resultat
ssh --timeout 300 -i sd:/Keys/id_tab5 user@serveur.example '/home/user/traitement.sh'
```

Single-quoting the remote command prevents premature expansion by the local terminal. Input bytes are sent to stdin; stdout can feed a pipe or redirection. stderr remains in the terminal. The remote command's exit status is preserved; local cancellation returns 130.

`--timeout SECONDS` sets the idle timeout from 1 to 3600 seconds, with a default of 30 seconds. DNS resolution and TCP connection have a 5-second limit; negotiation and authentication have a 20-second limit. A server that produces nothing within the selected timeout is interrupted even if its program is still working. Stopping closes the channel; what the remote program does after disconnection depends on the server and that program.

IPv4 addresses and DNS names are supported; internationalized names must use their Punycode form. IPv6, SSH tunnelling, an SSH agent, interactive PAM/MFA authentication, SCP and ANSI emulation are not provided by these commands.

## SFTP transfers

```sh
sftp ls -i sd:/Keys/id_tab5 user@serveur.example /home/user
sftp get -i sd:/Keys/id_tab5 user@serveur.example /home/user/document.txt sd:/Documents/document.txt
sftp put -i sd:/Keys/id_tab5 user@serveur.example /home/user/document.txt sd:/Documents/document.txt
```

The order is always **remote path, local path**. `get` receives; `put` sends. The same `-p`, `-i`, `--password-file`, `--passphrase-file` and `--timeout` options apply. The destination directory must already exist. Existing files are refused; choose another name to preserve the original.

Transfers use a temporary file with the `.tab5-sftp-part` suffix and rename it after success. Cancelling a download deletes its local temporary file. A connection interruption or cancellation during an upload may leave a remote temporary file: it remains explicitly incomplete and can be removed on the server before retrying. Pre-existing temporary files are not overwritten. A transfer never loads the entire file into RAM.

## Dependency, memory and validation

The sources of **libssh2 1.11.2_DEV**, under BSD-3-Clause, are included in `src/tab5ssh`, with `COPYING` and `UPSTREAM.json`. This is the official snapshot pinned to commit `18aebe08962c1878d84c373ad85caff25ec67771`, not a published stable 1.11.2 release. It includes memory and packet-validation fixes absent from the latest published release, 1.11.1. The choice and relevant commits are described in `MBEDTLS_BACKPORTS.md`. The cryptographic engine is mbedTLS supplied by the ESP32-P4 SDK; no Tab5OS-specific encryption is added.

Local adaptations cover Arduino/lwIP configuration, the memory profile and freeing a KEX-preference buffer when the next allocation fails. This leak was reproduced through allocation-failure injection and fixed without changing the algorithms. RSA keys are accepted with SHA-2 signatures while RSA/SHA-1 signatures remain disabled. Offered methods are ECDH P-256 or DH group14/SHA-256, ECDSA P-256 or RSA/SHA-2 host keys, AES-CTR and HMAC-SHA-256, with strict KEX when the server supports it.

Session allocations are capped at 512 KiB in PSRAM, with no SRAM fallback. Channel window: 64 KiB; packet: 16 KiB; application buffers: 2 KiB for SSH, 4 KiB for SFTP. The resource manager also admits the SDK's actual cryptographic memory budget. Internal mbedTLS allocations use the shared allocator installed by the OS; the command does not change this allocator globally.

The terminal stack is not enlarged for all commands. SSH is refused if the minimum free stack reported by FreeRTOS is below 8 KiB, notably within deeply nested scripts or substitutions. Running the command directly avoids that nesting depth without removing the protection. `tasks` shows actual stack measurements; `memory` shows available memory spaces and their largest free blocks.

Official sources: <https://libssh2.org/>, <https://github.com/libssh2/libssh2>, <https://libssh2.org/license.html>.

## Usage summary

`ssh` executes one remote command; it is not a PTY/VT100 emulator. Verify the complete host fingerprint independently, record it with `ssh trust HOST SHA256:...`, then use `ssh -i sd:/Keys/id_tab5 USER@HOST 'command'`. A changed host key is rejected before credentials are sent. `ssh forget HOST` removes the old pin after the server change has been checked. Hostname and port identify each pin separately.

Authentication accepts RSA/ECDSA PEM private keys or `--password-file FILE`; encrypted PEM keys may use `--passphrase-file FILE`. ECDSA requires the matching OpenSSH public-key file beside the private key, with `.pub` appended to its name. Secret file contents are not echoed or added to history, but files are not automatically encrypted on storage. Ed25519/OpenSSH private-key containers and interactive authentication are unsupported. Pipes transport stdin/stdout; stderr remains separate. `--timeout 1..3600` sets the idle timeout in seconds, default 30.

`sftp get|put [OPTIONS] USER@HOST REMOTE LOCAL` transfers one file; `sftp ls [OPTIONS] USER@HOST REMOTE` lists a directory. Existing files are never implicitly overwritten. Transfers stream through bounded buffers and use `.tab5-sftp-part` temporary files. An interrupted upload can leave a remote temporary file that must be removed before retrying. Close or Ctrl+C cancels the operation.
