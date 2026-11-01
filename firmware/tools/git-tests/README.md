# Git verification helpers

`run.py` builds and exercises the native command wrapper with installed PC Git. `run_pull.py` checks interrupted checkout recovery separately. Their native build requires the MSVC developer environment and the corresponding local libgit2 build.

`https_remote.py --client C:/path/to/git_cli.exe` runs the production command wrapper through libgit2's mbedTLS backend against a loopback HTTPS server backed by the installed `git http-backend`. The client must support the test-only `TAB5_GIT_TEST_CA` and `TAB5_GIT_CANCEL_MS` variables, and report its allocation counters. `--pc-only` validates the HTTPS fixture using PC Git without claiming to validate the embedded wrapper.

The client must have the companion `linked-sources.json` produced by `run.py`. The runner verifies its executable, source and library hashes before starting, then records the crypto version and linked inputs. The native backend and Arduino source have distinct hashes: the runner checks the vendor manifest and reconstructs the exact Arduino prefix and relative include substitutions before accepting their correspondence. Other source differences fail the test.

The HTTPS fixture requires Python's `cryptography` package, Git, and an available loopback port. It generates its CA, server key, credentials and independent repositories under `C:/workspace/tab5-git-0822`. None of those private test files belongs in the release archive. The server binds only to `127.0.0.1` and uses an ephemeral port; no external repository or service is contacted.

The flow checks authenticated clone, fetch, fast-forward pull, push with PC interoperability, refusal of a non-fast-forward push, certificate issuer and hostname failures, wrong credentials, interrupted pack download without moving references, retry, and cancellation during a delayed authenticated request. PC Git independently checks resulting repositories with `fsck --strict`. Source and client hashes must remain stable throughout a run.

The JSON report records actual elapsed time and tracked allocations after each command. These are host measurements. They do not establish ESP32-P4 SRAM usage, task-stack margins, physical storage behavior or the latency of the target's lwIP cancellation polling. The Windows backend may finish its current socket wait before noticing cancellation. Target compilation and device tests are separate gates.
