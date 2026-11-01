# Git memory regression tests — 0.8.31

Run `python3 tools/git-tests-0831/run.py` from the release directory. Requires Python 3 and GCC/G++ with AddressSanitizer and UndefinedBehaviorSanitizer.

The runner extracts the complete allocation/reallocation/free implementation, startup-ownership handoff and UI worker from production sources without rewriting those blocks. It compiles them with the actual `Tab5GitMemory.h`, `Tab5ResourceManager.cpp` and public policy headers. The capability-heap simulation reports SRAM, PSRAM, contiguous blocks and allocation failures; buffers use host malloc and are checked for data preservation. LeakSanitizer is disabled; explicit checks verify all tracked heap blocks and budget slots are released.

The new fixture reproduces the supplied values: a 40 KiB worker is admitted with 86 KiB of SRAM free, leaving 46 KiB; the subsequent 16 KiB session request plus 32 KiB safety margin fails. In 0.8.31 the session reservation is obtained before the worker stack admission, so the actual manager sees both demands while reclaiming idle SRAM. The test reclaimer simulates 12 KiB of recoverable memory; it does not pretend to run a device driver. The retained reservation survives the handoff without a second admission and prevents another application from consuming that headroom.

The exact UI worker executes against a deterministic Git backend for successful, failed and cancelled creation, cloning and existing-repository operations. Failed creation/cloning no longer runs an unrelated status operation or adds its misleading not-a-repository error. This backend does not implement cloning.

Prior coverage remains: progressive 256 KiB allocation batches, the 4 MiB versus old 8 MiB startup regression, exact-size fallback, concurrent application reservations, physical allocation failure, fragmentation, cancellation, overflow, 8 MiB total arena limit including headers and temporary realloc blocks, and 400 repeated sessions with full release.

The 40 KiB internal stack and 32 KiB system safety margin are unchanged. The TLS/adapter reserve remains conservative through the session: the SDK's TCP/SD/TLS allocations are not individually attributed to this reservation, and later requests can allocate again. This can count part of the untracked live working set conservatively twice; the new code does not guess that a handshake consumed a fixed number of bytes or release headroom speculatively.

These tests do not run the libgit2 clone protocol, real TLS, FreeRTOS scheduling, an SD card or a physical Tab5. The exact user's repository has not been exercised. Full firmware compilation and physical validation are separate evidence.
