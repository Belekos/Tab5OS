"""Rebuild only the pinned ESP-IDF 5.5.4 DSI/DW-GDMA units, cache-safe.

The installed SDK is unchanged. Archive/member names are intentional: the SDK
linker script uses them to place the DSI callback graph in internal IRAM.
"""
from pathlib import Path
import hashlib, json, shlex, subprocess

UPSTREAM = Path(__file__).with_name('dsi_cache_safe')
EXPECTED = {
    'esp_async_fbcpy.h': '85b24977d2f975d76a1e42ba15980a39ac185974d5abc3cf76320520879d7a0e',
    'esp_lcd_panel_dpi.c': '6d4a59514c1401334f1a4a89933611b259ca20ef09c9670fabfc37f1dea630da',
    'dw_gdma.c': 'ace979c51265cd782d061c1dc157530fb13f8e8cc834234df6c50eb2fe2a349f',
    'mipi_dsi_priv.h': 'ed692ae4f1d47c60f6130ddaa97ba708e777feedfaf1e77eaeb658cf2dab80e6',
}
CONFIG = '''#include "sdkconfig.h"
#include "esp_idf_version.h"
#if !defined(CONFIG_IDF_TARGET_ESP32P4) || ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5, 5, 4)
#error "Tab5 DSI cache-safe units require ESP32-P4 / ESP-IDF 5.5.4"
#endif
#if !CONFIG_LCD_DSI_OBJ_FORCE_INTERNAL || !CONFIG_LCD_DSI_ISR_HANDLER_IN_IRAM
#error "Existing SDK bus objects and linker mapping must already be internal"
#endif
#undef CONFIG_LCD_DSI_ISR_CACHE_SAFE
#define CONFIG_LCD_DSI_ISR_CACHE_SAFE 1
#undef CONFIG_LCD_DSI_OBJ_FORCE_INTERNAL
#define CONFIG_LCD_DSI_OBJ_FORCE_INTERNAL 1
#undef CONFIG_DW_GDMA_ISR_IRAM_SAFE
#define CONFIG_DW_GDMA_ISR_IRAM_SAFE 1
#undef CONFIG_DW_GDMA_OBJ_DRAM_SAFE
#define CONFIG_DW_GDMA_OBJ_DRAM_SAFE 1
'''

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def patch_sources():
    sources = {}
    for name, expected in EXPECTED.items():
        p = UPSTREAM / 'upstream' / name
        if sha(p) != expected:
            raise RuntimeError('Unexpected pinned DSI source: ' + name)
        sources[name] = p.read_text()
    dpi = sources['esp_lcd_panel_dpi.c']
    old = '''        // when an underrun happens, the LCD display may already becomes blue
        // it's too late to recover the display, so we just print an error message
        // as a hint to the user that he should optimize the memory bandwidth (with AXI-ICM)
        ESP_DRAM_LOGE(TAG, "can't fetch data from external memory fast enough, underrun happens");'''
    if dpi.count(old) != 1:
        raise RuntimeError('Unexpected native DSI underrun path')
    dpi = dpi.replace(old, '        tab5_display_underrun_isr();')
    dpi = dpi.replace('#include <sys/param.h>', '#include "tab5_dsi_cache_config.h"\n#include <sys/param.h>')
    dpi = dpi.replace('typedef struct esp_lcd_dpi_panel_t',
        '// Strong hook: supplied by Tab5DisplaySignals; no logging in the IRQ.\nextern void tab5_display_underrun_isr(void);\n\ntypedef struct esp_lcd_dpi_panel_t', 1)
    sources['esp_lcd_panel_dpi.c'] = dpi
    dw = sources['dw_gdma.c'].replace('#include <stdlib.h>', '#include "tab5_dsi_cache_config.h"\n#include <stdlib.h>', 1)
    dw = dw.replace('static const char *TAG = "dw-gdma";', 'static const DRAM_ATTR char TAG[] = "dw-gdma";')
    # Native ESP_RETURN_ON_FALSE[_ISR] diagnostics reference __FUNCTION__ and
    # formats in Flash. Keep the same error results and diagnostics, using
    # DRAM-only literals in the four functions called from the DSI EOF ISR.
    for name in ('dw_gdma_channel_enable_ctrl', 'dw_gdma_channel_use_link_list',
                 'dw_gdma_link_list_get_item', 'dw_gdma_lli_set_block_markers'):
        start = dw.index(name + '(')
        end = dw.index('\n}', start) + 2
        body = dw[start:end]
        import re
        pattern = r'ESP_RETURN_ON_FALSE(?:_ISR)?\((.*?),\s*(ESP_ERR_INVALID_ARG|ESP_ERR_INVALID_STATE|NULL),\s*TAG,\s*"([^"]+)"\);'
        body, changed = re.subn(pattern, lambda m:
            'do { if (!(' + m[1] + ')) { ESP_DRAM_LOGE(TAG, "' + name + ': ' + m[3] + '"); return ' + m[2] + '; } } while (0);', body, flags=re.S)
        expected = 2 if name in ('dw_gdma_channel_use_link_list', 'dw_gdma_link_list_get_item') else 1
        if changed != expected:
            raise RuntimeError('Unexpected native DSI helper guard: ' + name)
        dw = dw[:start] + body + dw[end:]
    sources['dw_gdma.c'] = dw
    sources['tab5_dsi_cache_config.h'] = CONFIG
    return sources

def prepare_dsi_cache_safe(sdk_root: Path, gcc: Path, work: Path, *,
                           compiler_flags=(), archiver=None, excluded_flags=()):
    sdk_root, gcc, work = Path(sdk_root).resolve(), Path(gcc).resolve(), Path(work).resolve()
    if work == sdk_root or work.is_relative_to(sdk_root):
        raise RuntimeError('Cache-safe objects must be built outside the installed SDK')
    target = work / 'dsi-cache-safe'
    src = target / 'src'; out = target / 'lib'
    src.mkdir(parents=True, exist_ok=True); out.mkdir(exist_ok=True)
    for name, text in patch_sources().items():
        (src / name).write_text(text)
    suffix = '.exe' if gcc.name.endswith('.exe') else ''
    ar = Path(archiver).resolve() if archiver else gcc.with_name('riscv32-esp-elf-ar' + suffix)
    native_flags = [f for f in shlex.split((sdk_root / 'flags/c_flags').read_text())
                    if f not in excluded_flags]
    commands = []; archives = []
    for name, library in [('esp_lcd_panel_dpi', 'libesp_lcd.a'), ('dw_gdma', 'libesp_hw_support.a')]:
        obj = out / (name + '.o')
        command = [str(gcc), *native_flags, *compiler_flags,
                   '-iprefix', str(sdk_root / 'include') + '/',
                   '@' + str(sdk_root / 'flags/includes'),
                   '@' + str(sdk_root / 'flags/defines'),
                   '-I' + str(sdk_root / 'dio_qspi/include'), '-I' + str(src),
                   '-I' + str(sdk_root / 'include/esp_lcd/interface'), '-Os',
                   '-fstack-usage', '-MMD', '-c', str(src / (name + '.c')), '-o', str(obj)]
        result = subprocess.run(command, capture_output=True, text=True)
        (target / (name + '-compile.log')).write_text(result.stdout + result.stderr)
        commands.append(command)
        if result.returncode:
            raise RuntimeError('Native cache-safe compilation failed: ' + name + '\n' + result.stderr)
        archive = out / library
        if archive.exists(): archive.unlink()
        subprocess.run([str(ar), 'rcs', str(archive), str(obj)], check=True)
        archives.append(archive)
    report = {'passed': True, 'sdk': str(sdk_root), 'version': 'ESP-IDF 5.5.4',
              'upstreamSha256': EXPECTED,
              'patchScriptSha256': sha(Path(__file__)),
              'generatedSha256': {p.name:sha(p) for p in sorted(src.iterdir())},
              'archives': {p.name:{'path':str(p),'sha256':sha(p)} for p in archives},
              'commands': commands,
              'excludedCompilerFlags': sorted(excluded_flags),
              'sdkArchiveSha256': {name:sha(sdk_root / 'lib' / name)
                                  for name in ('libesp_lcd.a', 'libesp_hw_support.a')},
              'physicalDeviceTested': False,
              'notes': ['Native IRQ allocation flags and native internal object capabilities are enabled in recompiled units.',
                        'Strong no-IO DSI underrun hook; task-context reporting is owned by the OS.',
                        'Archive and member names preserve SDK linker placement; verify the final ELF.',
                        'Panel timing, DMA transfer lengths and framebuffer allocation are unchanged.']}
    (target / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    return archives

if __name__ == '__main__':
    import argparse
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sdk-root', type=Path, required=True)
    p.add_argument('--gcc', type=Path, required=True)
    p.add_argument('--work', type=Path, required=True)
    a = p.parse_args()
    print(json.dumps([str(x) for x in prepare_dsi_cache_safe(a.sdk_root, a.gcc, a.work)]))
