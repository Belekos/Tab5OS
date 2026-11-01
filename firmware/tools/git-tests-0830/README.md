# Git memory regression tests — 0.8.30

Run `python3 tools/git-tests-0830/run.py` from the release directory. Requires Python 3 and GCC/G++ with AddressSanitizer and UndefinedBehaviorSanitizer.

The runner extracts the complete allocation, reallocation and free implementation and the startup budget line from the current production `Tab5Git.cpp`, without rewriting them. It compiles those bytes with the production `Tab5GitMemory.h`, `Tab5ResourceManager.cpp` and public policy headers. The simulated capability heap reports SRAM, PSRAM, contiguous block sizes and physical allocation failures; successful buffers are backed by host `malloc` and checked for data preservation. LeakSanitizer is disabled in this container; the tests explicitly check all heap blocks, allocator counters and reservation slots are released.

The regression fixture has 4 MiB of available PSRAM: the previous 8 MiB prerequisite rejects it, while the new startup admission and a modest Git allocation workload succeed. Further checks cover admission batching, exact-size fallback, another application's pending memory, preserved system margins, physical allocation failure, fragmentation, cancellation, size overflow, the unchanged 8 MiB arena quota and resize peaks. Four hundred repeated sessions check release of heap bytes and budget slots.

The 40 KiB internal task stack is unchanged. Tests retain both the task admission thresholds and TLS startup thresholds. The TLS/startup reservation remains conservative throughout the operation; it is not released speculatively after a handshake.

This does **not** run libgit2's clone protocol, real TLS, FreeRTOS, a physical SD card or a Tab5. No specific user's repository has been reproduced. The result establishes the memory-admission regression and allocator behavior; full target compilation and physical validation are separate evidence.
