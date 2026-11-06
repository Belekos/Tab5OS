# Vault and security

## Opening and locking

The **device vault** opens natively, without Python or slow key derivation. On first use, create the required PIN, then any file keys you need. A key named Personnel is no longer recreated automatically: every key, including the first, can be deleted after confirmation.

Access requires a **4- to 12-digit PIN** from first use. Updating a device vault that has no PIN also requires creating one, without replacing its master key or entries. Changing the PIN requires the current PIN; an empty field no longer removes protection. The PIN is requested whenever a locked vault is reopened. After three failures, an increasing delay slows further attempts during the session. Closing the vault or turning off the screen clears the session and password cache in RAM; new reads by the shell, Python, Git and native applications are refused. An application that has already received a value may retain its own copy.

The random master key remains in internal NVS memory, as in earlier versions. **Encryption protects a removed SD card. The PIN controls access through the interface and normal APIs; it does not protect against internal flash extraction or modified firmware.** No eFuses, Secure Boot or flash encryption are programmed. Native/ELF programs share memory and are trusted code.

## Keys and files

Encryption creates a `.t5v` copy and preserves the original. Decryption creates an ordinary copy. Closing the vault does not make originals, decrypted copies or open documents confidential. Limit: 128 MiB per file; filenames and sizes remain visible.

Deleting a key removes it from the current keyring. It does not delete encrypted files or backups of that key. Without another usable copy, files associated with the key become unrecoverable. Deleting all file keys does not destroy password-vault entries: these use a separate key derived from the device master key.

Two backup types are available:

| Type | Purpose | Restoration |
|---|---|---|
| Device | Fast backup without a file password | With the same device master key, after opening the vault |
| Portable | AES-GCM export protected by a password of at least 12 characters | On another Tab5 using the password, even after losing the original device |

A device backup is tied to this Tab5; it does not replace a portable export for surviving loss, replacement or erasure of internal memory. Portable exports and older backups remain compatible: PBKDF2-HMAC-SHA-256, 600,000 iterations and a random salt. This derivation remains costly, but runs in a background task with cancellation checks during calculation. Device backup does not perform it. Cancel leaves the original intact and removes the temporary file.

## Legacy password vault

Older keyrings retain their original format and password. An update cannot recover a forgotten password. In Vault access, open the old vault, then copy its keys to the device vault. If the device already has a PIN, enter it to authorize the copy. The merge rejects identifier collisions or more than eight keys and preserves the old keyring. Later device-vault changes do not modify that older keyring.

## Passwords, tokens and environment

The vault stores up to **64 entries**, with an ASCII variable name of at most 48 characters and a UTF-8 value of at most 512 bytes. Names follow `[A-Za-z_][A-Za-z0-9_]*`. Value input fields do not use history or the clipboard. Variable and Git configuration references contain names; values remain in the encrypted vault. See [ENVIRONNEMENT_ET_COFFRE.md](ENVIRONNEMENT_ET_COFFRE.md) for the shell, MicroPython and ELF interfaces.

Storage uses two alternating encrypted files under `/System/Secrets/`. A new version is written to a temporary file, closed and fully verified before adoption. The previous authenticated version is retained for recovery after an interrupted write. Vault names and values are never written to the SD card in plaintext. Reads do not rewrite files; saving an identical value does not create a new version.

These entries are **tied to the device master key**. Copying both SD files allows restoration on the same intact device; portable file-key exports do not export this master key or the passwords. Losing the master key makes the entries unreadable. Earlier states remain encrypted in the backup bank; SD/NVS wear leveling prevents any guarantee of physical erasure. The format does not claim protection against restoring an older valid encrypted state.

## Technical verification

AES-256-GCM and HMAC-SHA-256 primitives come from mbedTLS. Each file block authenticates its position and the header; decryption verifies every block before publishing the decrypted copy. The device master key wraps its NVS keyring. Distinct HMAC domains separate local backups and passwords. Entropy comes from the CTR-DRBG seeded from the P4; a generation failure blocks the operation without a predictable fallback value.

## Network connections

Internet TLS connections now use a common client with the ESP-IDF certificate bundle. Certificate and hostname verification is no longer disabled for maps, Wikipedia archives, HTTPS media streams, Python services or TLS IRC. The date must be initialized; the client rejects a clock earlier than November 2023. This threshold detects an unsynchronized device but does not by itself guarantee an accurate clock: synchronization is still required for certificate validity dates.

Retained applications can still use an explicitly configured HTTP URL; that connection remains unencrypted. Local printers retain a separate discovery path followed by certificate validation/storage. Temporarily disabling verification during that discovery alone must not be equated with an unchecked Internet client.

The certificate store is the one supplied with core 3.3.9/IDF 5.5.4, not a continuously updated set of certificate authorities. A server with a self-signed certificate, an unknown chain or an invalid date may therefore be rejected. No security certification or independent audit is claimed.

## Inaccessible vault

The locked page offers **Cannot access the vault?**, with a warning and a requirement to type `RESET`. Resetting deletes this Tab5's master key and keys; encrypted credentials are archived on SD but remain inaccessible. Encrypted files and the legacy password vault are preserved. Portable key backups are still required to decrypt the corresponding files.

## Loading after unlock

PIN validation and credential loading have separate states. Read, memory or decryption errors remain visible; the interface does not silently retry reading the banks. Reinsert the SD card and choose **Retry** to make another explicit background attempt. This process preserves existing files and keys.

Vault diagnostics retain only the technical stage in RTC memory to help investigate a possible restart. They store no PIN, credential name or secret value there, and do not write periodically to SD.
