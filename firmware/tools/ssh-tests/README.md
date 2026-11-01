# SSH verification

This harness links the production SSH module to the bundled libssh2 sources
and actual mbedTLS sources, then exchanges encrypted SSH/SFTP traffic with a
local Paramiko server. It does not execute arbitrary operating-system commands
on that server and never contacts an external SSH service.

Run from a Windows x64 Visual Studio Developer PowerShell with Python 3.12+
and Paramiko 4.0.0 installed in its environment. Use the exact mbedTLS 3.6.5
submodule pinned by ESP-IDF 5.5.4, commit
`ffb280bb63c78bfec1e1ab55040671768c85c923` from
<https://github.com/espressif/mbedtls>. Set `MBEDTLS_ROOT` to that source
directory (it contains `include` and `library`). Set `TAB5_SSH_TEST_DIR` to an isolated,
writable build directory. `TAB5_SSH_TEST_DEPS` optionally points at a separate
Python dependency directory.

```powershell
git init C:/deps/mbedtls-tab5
git -C C:/deps/mbedtls-tab5 fetch --depth=1 https://github.com/espressif/mbedtls.git ffb280bb63c78bfec1e1ab55040671768c85c923
git -C C:/deps/mbedtls-tab5 checkout --detach FETCH_HEAD
$env:MBEDTLS_ROOT = 'C:/deps/mbedtls-tab5'
$env:TAB5_SSH_TEST_DIR = 'C:/workspace/tab5-ssh-tests'
python tools/ssh-tests/build_host.py
python tools/ssh-tests/run.py
```

The build directory contains native objects, a test client, server logs, and
generated fixture credentials. They are test data, never user credentials.
The host adapter models POSIX byte-preserving files and stores host pins in
fixture files. This verifies pin policy and persistence logic, not ESP32 NVS
hardware. Native mbedTLS uses the same core source commit as the target SDK
but its Windows configuration, software platform and allocator; its binary
is not identical to the firmware. The earlier 3.6.6/ESP-IDF5.5.5 native run
is preserved separately in `ssh-0822-mbedtls366.json` as historical evidence.
The second gate
compiles against the actual ESP32-P4 SDK configuration:

```powershell
python tools/ssh-tests/compile_target.py --compile-db C:/path/to/compile_commands.json --output C:/workspace/tab5-ssh-target
```

The compile database must contain the project's `Tab5Tools.cpp` translation
unit and working paths to the installed P4 SDK/compiler. No flashing or final
firmware link occurs in this check.

Reports are written to `tools/verification/ssh-0822.json` and
`ssh-target-0822.json`. The protocol gate covers trust before credentials,
password and PEM keys, separate stdin/stdout/stderr, large payloads, remote
status, timeouts, cancellation, SFTP partial files, failing CLOSE, repeated
sessions, and allocation failures through the observed session-allocation
boundaries. It checks both the session allocator and native CRT memory after
warm-up. SDK-internal allocation failures, physical SRAM/PSRAM fragmentation,
Wi-Fi latency and physical stack watermarks still require a tablet.
