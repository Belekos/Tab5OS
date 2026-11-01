# Explorer navigation-tree checks — 0.8.38

Run from the release directory:

```sh
python3 tools/explorer-tree-tests-0838/run.py
```

The runner copies the production `Tab5ExplorerTree.h`, `Tab5DirectoryModel.h` and `Tab5FileSort.h` unchanged, compiles with AddressSanitizer and UndefinedBehaviorSanitizer, and checks that the source hashes remain stable during the run. Filesystem objects, hidden-file registry, USB presence, time, and ESP32 PSRAM allocation are deterministic host doubles.

Covered behavior:

- Only directories enter the tree; files incur no `size()`/`getLastWrite()` call.
- SD and mounted USB are independent selectable root rows. SD initially expands, or USB when it is the sole available support. Hotplug preserves the other support's expanded branches.
- Collapsing SD cannot interrupt a USB scan or child-presence probe.
- Row indexes, six sort modes, 8,193 folders, hidden entries, 129 levels, missing paths, removal and resumed scans.
- Visible-row child probes stop at the first eligible subfolder, otherwise inspect cooperatively. Their results are cached and refreshed after an explicit filesystem refresh. Files-only folders remain selectable without a chevron. Unrequested descendants are not recursively inspected.
- Probe cancellation, duplicate requests, a regular expansion taking over an active probe without retaining two iterators, collapse/re-expand, metadata release preserving expanded paths, and allocation refusal during both scanning and support changes.
- Cancelling a settled tree preserves its branches and row indexes without marking structure/hints dirty or accessing storage; 200,000 settled service calls perform no filesystem reads/opens or tracked PSRAM growth; direct row lookup avoids hidden-registry lookups.

`results.txt` and `../verification/explorer-tree-0838.json` record the execution and source/evidence hashes. The benchmark is a host measurement, not ESP32-P4 latency or a tablet frame-rate result. LeakSanitizer is disabled because the host environment restricts its process inspection; every tracked PSRAM allocation and file handle must nevertheless be released explicitly.
