from pathlib import Path
import hashlib,json,os,re,shutil,subprocess,sys
here=Path(__file__).resolve().parent;release=here.parents[1];work=release.parent
sketch=release/'Tab5OS_Arduino_v0.8.22_Ecriture'
upstream=Path(sys.argv[1]) if len(sys.argv)>1 else sketch/'src/tab5git'
build=Path('C:/workspace/tab5-git-regexp-0822');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
 cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'" "'+str(upstream)+'"\n',encoding='utf-8');raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
pcre_source=upstream/'deps/pcre2';pcre=build/'vendor/deps/pcre2';pcre.mkdir(parents=True,exist_ok=True)
embedded_config=upstream/'Tab5GitEmbeddedConfig.h'
if embedded_config.exists():shutil.copy2(embedded_config,build/'vendor/Tab5GitEmbeddedConfig.h')
for source in pcre_source.iterdir():
 if source.suffix in ('.c','.h'):shutil.copy2(source,pcre/source.name)
# The vendored includes are relative; use the native configuration alongside
# unchanged engine sources rather than accidentally selecting SDK builtins.
config=here/'regexp_config.h';shutil.copy2(config,pcre/'config.h')
regexp=upstream/'src/util/regexp.c';text=regexp.read_text(encoding='utf-8');start=text.index('#elif defined(GIT_REGEX_BUILTIN) || defined(GIT_REGEX_PCRE2)');start=text.index('\n',start)+1;end=text.index('#elif defined(GIT_REGEX_REGCOMP)',start)
header='''#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
typedef ptrdiff_t ssize_t;
typedef pcre2_code *git_regexp;
typedef struct {ssize_t start,end;} git_regmatch;
void *quota_malloc(size_t);void quota_free(void*);void regexp_error(const char*);
#define git__malloc quota_malloc
#define git__free quota_free
#define GIT_UNUSED(x) (void)(x)
#define GIT_ASSERT(x) assert(x)
#define GIT_REGEXP_ICASE 1
#define GIT_ERROR_REGEX 1
#define GIT_EINVALIDSPEC -12
#define GIT_ENOTFOUND -3
#define git_error_set_str(c,s) regexp_error(s)
#define git_error_set_oom() regexp_error("oom")
'''
(build/'controller.c').write_text(header+text[start:end],encoding='utf-8')
names='''pcre2_auto_possess.c pcre2_chartables.c pcre2_chkdint.c pcre2_compile.c
pcre2_compile_cgroup.c pcre2_compile_class.c pcre2_config.c pcre2_context.c
pcre2_convert.c pcre2_dfa_match.c pcre2_error.c pcre2_extuni.c pcre2_find_bracket.c
pcre2_maketables.c pcre2_match.c pcre2_match_data.c pcre2_match_next.c pcre2_newline.c
pcre2_ord2utf.c pcre2_pattern_info.c pcre2_script_run.c pcre2_serialize.c
pcre2_string_utils.c pcre2_study.c pcre2_substitute.c pcre2_substring.c pcre2_tables.c
pcre2_ucd.c pcre2_valid_utf.c pcre2_xclass.c'''.split()
sources=[pcre/n for n in names]+[build/'controller.c']
flags=['/nologo','/O2','/MD','/TC','/std:c11','/utf-8','/D_CRT_SECURE_NO_WARNINGS','/DHAVE_CONFIG_H','/DPCRE2_CODE_UNIT_WIDTH=8','/DPCRE2_STATIC','/DPCRE2_EXPORT=','/DTAB5_GIT_EMBEDDED_PROFILE','/I'+str(build),'/I'+str(pcre)]
r=subprocess.run(['cl',*flags,'/c',*[str(p) for p in sources]],cwd=build,capture_output=True,encoding='utf-8',errors='replace')
if r.returncode:
 (build/'compile.log').write_text(r.stdout+r.stderr,encoding='utf-8');print((r.stdout+r.stderr)[:5000]);raise SystemExit(r.returncode)
objects=[str(build/(p.stem+'.obj')) for p in sources]
r=subprocess.run(['cl','/nologo','/O2','/MD','/EHsc','/std:c++17','/utf-8','/DPCRE2_STATIC','/I'+str(pcre),str(here/'regexp.cpp'),*objects,'/Fe:'+str(build/'regexp.exe')],cwd=build,capture_output=True,encoding='utf-8',errors='replace')
if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(build/'regexp.exe')],capture_output=True,encoding='utf-8',errors='replace',timeout=40);print(r.stdout+r.stderr)
tracked=[regexp]+[pcre_source/n for n in names]+list(pcre_source.glob('*.h'))
if embedded_config.exists():tracked.append(embedded_config)
report=dict(passed=r.returncode==0,output=r.stdout+r.stderr,physicalDeviceTested=False,method='Actual PCRE2 engine and unchanged production regexp controller block. Embedded initial-workspace profile, quota callbacks, fail-each-allocation compilation and match/search OOM, nesting/length/backtracking limits, cancellation callout; no tablet timing measurement.',upstream=str(upstream),sources={p.relative_to(sketch).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked} if upstream==sketch/'src/tab5git' else {},upstreamSources={p.relative_to(upstream).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked},tests={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__),here/'regexp.cpp',config]},hostConfigSha256=hashlib.sha256(config.read_bytes()).hexdigest())
(release/'tools/verification/git-regexp-quota-0822.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');raise SystemExit(r.returncode)
