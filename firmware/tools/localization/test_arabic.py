"""Run real LVGL 9.6 Arabic shaping with injected allocation failures."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from datetime import datetime, timezone

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS / 'build_clang'))
sys.path.insert(0, str(TOOLS))
from prepare_lvgl import patch_shaper
from audit_languages import glyphs


def function(source, name, returns='uint32_t'):
    start = source.rfind('static ' + returns + ' ' + name + '(')
    assert start >= 0
    end = source.index('\n}', start) + 2
    return source[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lvgl', required=True, type=Path)
    parser.add_argument('--sketch', required=True, type=Path)
    parser.add_argument('--cxx', default='cl')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    version = re.search(r'_v(\d+\.\d+\.\d+)_', args.sketch.name).group(1)
    if args.output is None:
        args.output = TOOLS / 'verification' / ('arabic-' + version.replace('.', '') + '.json')
    original = (args.lvgl / 'src/misc/lv_text_ap.c').read_bytes()
    patched, _, _ = patch_shaper(original)
    shaper = re.sub(r'^#include.*$', '', patched.decode('utf-8'), flags=re.M)
    bidi_original = (args.lvgl / 'src/misc/lv_bidi.c').read_bytes()
    bidi = re.sub(r'^#include.*$', '', bidi_original.decode('utf-8'), flags=re.M)
    utf8 = (args.lvgl / 'src/misc/lv_text.c').read_text(encoding='utf-8')
    macros = '\n'.join(line for line in utf8.splitlines() if line.startswith('#define LV_IS_'))
    samples = ['', 'Open', 'العربية', 'لا', 'إعدادات الملفات', 'ملف sd:/Dossier/fichier.ext', 'USB · 123 MiB', 'العربية\nEnglish', 'آ أ إ ئ ة ى ؤ ٠١٢٣٤٥٦٧٨٩']
    pack = args.sketch / 'sdcard/System/languages/ar.json'
    pack_values = json.loads(pack.read_text(encoding='utf-8'))['translations'] if pack.exists() else {}
    samples += list(pack_values.values())
    literals = ',\n'.join(json.dumps(s, ensure_ascii=False) for s in samples)
    source = r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
static unsigned allocation_ordinal = 0, failure_mask = 0, outstanding = 0;
static void * lv_malloc(size_t size) {
    if(failure_mask & (1u << allocation_ordinal++)) return NULL;
    void * result = malloc(size); assert(result); ++outstanding; return result;
}
static void lv_free(void * pointer) { if(pointer) {assert(outstanding); --outstanding; free(pointer);} }
#define LV_USE_ARABIC_PERSIAN_CHARS 1
#define LV_UNDEF_ARABIC_PERSIAN_CHARS UINT32_MAX
#define LV_AP_ALPHABET_BASE_CODE 0x0622
#define LV_AP_END_CHARS_LIST {0,0,0,0,0,{0,0}}
#define LV_ASSERT_NULL(value) assert(value)
#define lv_text_encoded_next lv_text_utf8_next
#define lv_text_encoded_prev lv_text_utf8_prev
#define lv_text_encoded_size lv_text_utf8_size
#define lv_text_get_encoded_length lv_text_utf8_get_length
#define lv_memzero(pointer, size) memset(pointer, 0, size)
#define lv_memcpy memcpy
#define LV_USE_BIDI 1
#define LV_BIDI_BASE_DIR_DEF LV_BASE_DIR_AUTO
typedef enum { LV_BASE_DIR_LTR=0x00, LV_BASE_DIR_RTL=0x01, LV_BASE_DIR_AUTO=0x02,
               LV_BASE_DIR_NEUTRAL=0x20, LV_BASE_DIR_WEAK=0x21 } lv_base_dir_t;
lv_base_dir_t lv_bidi_detect_base_dir(const char * txt);
void lv_bidi_process_paragraph(const char *, char *, uint32_t, lv_base_dir_t, uint16_t *, uint16_t);
'''
    source += macros + '\n' + function(utf8, 'lv_text_utf8_size', 'uint8_t') + '\n' + function(utf8, 'lv_text_utf8_next') + '\n' + function(utf8, 'lv_text_utf8_prev') + '\n' + function(utf8, 'lv_text_utf8_get_length') + '\n' + shaper + '\n' + bidi
    source += '\nstatic const char * samples[] = {\n' + literals + '\n};\n'
    source += r'''
int main() {
    for(size_t index = 0; index < sizeof(samples) / sizeof(samples[0]); ++index) {
        const char * input = samples[index];
        size_t capacity = lv_text_ap_strlen(input) + 1;
        assert(capacity >= strlen(input) + 1);
        for(unsigned failure = 0; failure <= 3; ++failure) {
            for(unsigned in_place = 0; in_place <= 1; ++in_place) {
                unsigned char * buffer = (unsigned char *)malloc(capacity + 32);
                assert(buffer); memset(buffer, 0xA7, capacity + 32);
                char * out = (char *)buffer + 16;
                if(in_place) memcpy(out, input, strlen(input) + 1);
                failure_mask = failure; allocation_ordinal = 0;
                lv_text_ap_proc(in_place ? out : input, out);
                assert(outstanding == 0);
                assert(memchr(out, 0, capacity));
                for(unsigned n = 0; n < 16; ++n) {
                    assert(buffer[n] == 0xA7);
                    assert(buffer[capacity + 16 + n] == 0xA7);
                }
                if(failure) assert(strcmp(out, input) == 0);
                else {
                    if(index == 1) assert(strcmp(out, "Open") == 0);
                    if(index == 2) assert(strcmp(out, "ﺍﻟﻌﺮﺑﻴﺔ") == 0);
                    if(index == 3) assert(strcmp(out, "ﻻ") == 0);
                    if(index == 5) assert(strstr(out, "sd:/Dossier/fichier.ext"));
                    if(!in_place) {
                        size_t visual_capacity = strlen(out) + 1;
                        unsigned char * visual_buffer = (unsigned char *)malloc(visual_capacity + 32);
                        assert(visual_buffer); memset(visual_buffer, 0xA7, visual_capacity + 32);
                        char * visual = (char *)visual_buffer + 16;
                        lv_bidi_process(out, visual, LV_BASE_DIR_AUTO);
                        assert(memchr(visual, 0, visual_capacity));
                        assert(strlen(visual) == strlen(out));
                        for(unsigned n = 0; n < 16; ++n) {
                            assert(visual_buffer[n] == 0xA7);
                            assert(visual_buffer[visual_capacity + 16 + n] == 0xA7);
                        }
                        if(index == 1) assert(strcmp(visual, "Open") == 0);
                        if(index == 2) {
                            assert(lv_bidi_detect_base_dir(out) == LV_BASE_DIR_RTL);
                            assert(strcmp(visual, "ﺔﻴﺑﺮﻌﻟﺍ") == 0);
                        }
                        if(index == 5) assert(strcmp(visual, "sd:/Dossier/fichier.ext ﻒﻠﻣ") == 0);
                        printf("%zu", index);
                        for(uint32_t offset = 0; visual[offset];) printf(" %04X", lv_text_encoded_next(visual, &offset));
                        puts("");
                        free(visual_buffer);
                    }
                }
                free(buffer);
            }
        }
    }
    return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix='tab5-arabic-') as temporary:
        root = Path(temporary)
        cpp = root / 'arabic.c'
        cpp.write_text(source, encoding='utf-8')
        exe = root / 'arabic.exe'
        command = [args.cxx, '/nologo', '/utf-8', '/std:c11', str(cpp), '/Fe:' + str(exe)]
        compiled = subprocess.run(command, cwd=root, capture_output=True, text=True)
        if compiled.returncode:
            print(compiled.stdout + compiled.stderr)
            raise SystemExit(compiled.returncode)
        result = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True)
        if result.returncode:
            print(result.stdout + result.stderr)
            raise SystemExit(result.returncode)
    supported = glyphs(args.sketch / 'sdcard/System/font.ttf')
    unsupported = {}
    for row in result.stdout.splitlines():
        items = row.split()
        bad = {int(value, 16) for value in items[1:] if int(value, 16) > 31 and int(value, 16) not in supported}
        if bad:
            unsupported[items[0]] = ['U+%04X' % value for value in sorted(bad)]
    report = {'lvglVersion': '9.6.0', 'originalSha256': hashlib.sha256(original).hexdigest(), 'patchedSha256': hashlib.sha256(patched).hexdigest(), 'bidiSha256': hashlib.sha256(bidi_original).hexdigest(), 'sampleCount': len(samples), 'arabicPackEntries': len(pack_values), 'checks': ['real LVGL UTF-8 decoder, Arabic shaper and BiDi algorithm', 'normal shaping with lam-alef', 'first, second and both allocations fail', 'in-place and distinct output buffers', 'exact lv_text_ap_strlen capacity with 32 canary bytes', 'no leaked allocations', 'Arabic visual order and Latin path preserved by BiDi AUTO', 'BiDi output capacity with 32 canary bytes'], 'unsupportedShapedGlyphs': unsupported, 'physicalDisplayTested': False, 'pass': not unsupported}
    report.update({'release':version,'testedAt':datetime.now(timezone.utc).isoformat(),'packSha256':hashlib.sha256(pack.read_bytes()).hexdigest(),'fontSha256':hashlib.sha256((args.sketch/'sdcard/System/font.ttf').read_bytes()).hexdigest()})
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False, indent=2))
    raise SystemExit(1 if unsupported else 0)


if __name__ == '__main__':
    main()
