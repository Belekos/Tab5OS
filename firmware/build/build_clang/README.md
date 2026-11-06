# Windows / Espressif Clang

This historical build path remains experimental for Tab5OS 0.8.53. It has not been rerun and does not yet reproduce all cache and allocator-detection settings used by the GCC builds. The [Linux/GCC build](../../../docs/BUILD.md#linux-x86_64) is the reference.

Install Arduino CLI, M5Stack core 3.3.9, GCC esp-rv32/2601, Espressif Clang esp-20.1.1_20250829 and the library versions listed in the [guide](../../COMPILATION.md). From `firmware`, adjust the paths:

```powershell
python build/build_clang/compile_tab5_clang.py --release-root . --libraries-dir C:/Tab5Deps --work-dir C:/Tab5Build --arduino-data C:/Users/YOUR_ACCOUNT/AppData/Local/Arduino15
```

`--arduino-cli`, `--clang-root` and `--gcc-root` select the tools. `--clang-root` names the directory containing `bin/clang.exe`. GCC headers and libraries are still required. The work directory must be outside the sketch.

`--mode prepare` prepares the arguments; `--mode database` creates the compilation database; `build` mode compiles. The scripts do not download Clang or flash the Tab5.

The preparation scripts verify hashes and modify isolated copies. `ctags_declared.py` checks sketch declarations with Clang. `normalize_clang_deps.py` normalizes verified dependency paths. The old checkers tied to local 0.8.28 paths are no longer retained.
