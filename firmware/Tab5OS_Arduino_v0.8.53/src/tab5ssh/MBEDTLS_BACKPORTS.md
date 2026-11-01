# libssh2 source selection for Tab5OS 0.8.22

The bundled source is the complete official **1.11.2_DEV** snapshot at
[`18aebe08962c1878d84c373ad85caff25ec67771`](https://github.com/libssh2/libssh2/tree/18aebe08962c1878d84c373ad85caff25ec67771).
It is a pinned development snapshot, not a published stable 1.11.2 release.

This file retains its initial review filename. The final source does **not**
consist of libssh2 1.11.1 plus local mbedTLS backports. The preliminary 1.11.1
integration exposed allocator mismatches during real ECDH/PEM tests and a
retained packet allocation during fault injection. Reviewing upstream history
also identified transport memory-safety corrections. Importing the complete
pinned source avoids a partial, interdependent set of security backports.

Examples of official corrections included in this snapshot:

| Official commit | Correction |
| --- | --- |
| [f7fa81ca5956610c0e3e17caba560e30309a4470](https://github.com/libssh2/libssh2/commit/f7fa81ca5956610c0e3e17caba560e30309a4470) | mbedTLS allocator ownership, OOM cleanup and PEM termination |
| [62a2a7ec2c2d408d579c4d8b255678a988c418ad](https://github.com/libssh2/libssh2/commit/62a2a7ec2c2d408d579c4d8b255678a988c418ad) | ECDSA signature allocator ownership |
| [823a2286404611b4cc58729e6fed04626af932ed](https://github.com/libssh2/libssh2/commit/823a2286404611b4cc58729e6fed04626af932ed) | Transport payload leak and potential double free |
| [42e33d81577ed4b95d4b4f6f845e5ee8efe5eeb4](https://github.com/libssh2/libssh2/commit/42e33d81577ed4b95d4b4f6f845e5ee8efe5eeb4) | Potential heap overflow during encrypt-then-MAC decryption |
| [97acf3dfda80c91c3a8c9f2372546301d4a1a7a8](https://github.com/libssh2/libssh2/commit/97acf3dfda80c91c3a8c9f2372546301d4a1a7a8) | Additional transport packet-length checks |

`UPSTREAM.json` records original source hashes and the four remaining local
changes: selecting the ESP32 configuration, the configuration itself, and
smaller default channel windows/packets, plus releasing `newprefs` when the
following extended-KEX preference allocation fails. The latter leak was
reproduced at allocation boundary 3 and checked through repeated fault
injections; it changes ownership cleanup only. No cryptographic implementation is
modified locally. `COPYING` preserves the upstream BSD-3-Clause license.

The OS uses session allocation callbacks with a strict PSRAM budget. Internal
mbedTLS allocations use the existing SDK allocator and OS crypto admission
budget. No global allocator is replaced by the command. Test reports under
`tools/verification` separate actual native mbedTLS/SSH exchanges, ESP32-P4
object compilation, and the still-required physical-device measurements.
