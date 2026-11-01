"""Exercise the real shell lexer/help/errors and terminal locale adapter on host."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sketch', required=True, type=Path)
    parser.add_argument('--cxx', default='cl')
    args = parser.parse_args()
    shell = (args.sketch / 'Tab5Shell.cpp').read_text(encoding='utf-8')
    terminal = (args.sketch / 'Tab5Terminal.cpp').read_text(encoding='utf-8')
    pack = json.loads((args.sketch / 'sdcard/System/languages/fr-FR.json').read_text(encoding='utf-8'))['translations']
    delta = json.loads((Path(__file__).parent / 'delta-shell-0817.json').read_text(encoding='utf-8'))
    help_entries=[]
    for row in (args.sketch/'Tab5ShellHelp.h').read_text(encoding='utf8').splitlines():
        row=row.strip().rstrip(',')
        if row.startswith('{"'):help_entries.append(json.loads('['+row[1:-1]+']'))
    for path in (args.sketch/'sdcard/System/languages').glob('*.json'):
        translations=json.loads(path.read_text(encoding='utf8'))['translations']
        for entry in help_entries:assert entry[2] in translations,(path.name,entry[0])
    inventory={r['source'] for r in delta}|{r[2] for r in help_entries}
    inventory.update(re.findall(r'\btr\("([^"\\]*)"\)',shell[shell.index('int Engine::printHelp'):shell.index('bool Engine::lex')]))
    mapping = ',\n'.join('{' + json.dumps(k, ensure_ascii=False) + ',' + json.dumps(pack[k], ensure_ascii=False) + '}' for k in sorted(inventory))
    functions = '\n'.join(line for line in shell.splitlines() if line.startswith(('Engine::Engine(', 'int Engine::error(')))
    functions += '\n' + shell[shell.index('const char* Engine::help('):shell.index('std::string Engine::value(')]
    adapter = next(line for line in terminal.splitlines() if line.startswith('std::string tr('))
    # Assert that the real dynamic paths and command suffix use translation before
    # concatenation; their user values must not be fed to a prefix replacement.
    assert not re.search(r'\berror\("(?:\\.|[^"\\])*"\+', shell)
    assert 'error(tr("unknown command: ")+c+tr(" (help lists commands)"))' in shell
    source = r'''
#include <cassert>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>
#include <functional>
#include <utility>
#include <map>
#include <iostream>
#define private public
'''
    source += '#include "' + (args.sketch / 'Tab5Shell.h').resolve().as_posix() + '"\n#undef private\n'
    source += '#include "' + (args.sketch / 'Tab5ShellHelp.h').resolve().as_posix() + '"\n'
    source += 'static const std::map<std::string,std::string> french = {\n' + mapping + '\n};\n'
    source += r'''
struct Locale {
    size_t copyText(const char * source, char * out, size_t capacity) {
        auto found = french.find(source);
        std::string value = found == french.end() ? source : found->second;
        if(out && capacity) {
            size_t count = value.size() < capacity - 1 ? value.size() : capacity - 1;
            memcpy(out, value.data(), count); out[count] = 0;
        }
        return value.size();
    }
} Tab5Locale;
'''
    source += adapter + '\nnamespace Tab5Shell {\n' + functions + '\n}\n'
    source += r'''
int main() {
    for(const auto& row : french) assert(tr(row.first.c_str()) == row.second);
    std::string output;
    Tab5Shell::Engine shell("/sd", [&](const char * data,size_t size){output.append(data,size);});
    shell.translate(tr);
    std::vector<Tab5Shell::Engine::Token> tokens;
    assert(!shell.lex("echo |",tokens));
    assert(output == "erreur : opérateur non pris en charge : |\n");
    output.clear(); tokens.clear();
    assert(!shell.lex("echo 'unfinished",tokens));
    assert(output == "erreur : guillemet ou échappement non terminé\n");
    output.clear();
    const std::string path = "/Documents/invalid path — error: .txt";
    shell.error(shell.tr("cannot read ") + path);
    assert(output == "erreur : impossible de lire " + path + "\n");
    output.clear();
    const std::string command = "unknown-user-command";
    shell.error(shell.tr("unknown command: ")+command+shell.tr(" (help lists commands)"));
    assert(output == "erreur : commande inconnue : unknown-user-command (help liste les commandes)\n");
    output.clear();
    shell.out("invalid path\n");
    assert(output == "invalid path\n"); // Documents and command output remain exact.
    for(const auto& entry : Tab5Shell::helpEntries) {
        const char * help = Tab5Shell::Engine::help(entry.command);
        assert(french.find(help) != french.end());
        assert(shell.tr(help) == french.at(help));
        output.clear();assert(shell.printHelp(entry.command)==0);
        assert(output.rfind(entry.syntax,0)==0);
        assert(output.find(french.at(help))!=std::string::npos);
        assert(output.find(entry.example)!=std::string::npos);
    }
    output.clear();assert(shell.printHelp(nullptr)==0);
    assert(output.find("Réseau et téléchargements")!=std::string::npos);
    assert(output.find("Ctrl+A/C/X/V/Z/Y")!=std::string::npos);
    std::cout << "PASS shell: 55 complete help pages available in all 14 packs, real French printHelp, lexer errors, dynamic paths and commands, untouched user output\n";
}
'''
    with tempfile.TemporaryDirectory(prefix='tab5-shell-locale-') as tmp:
        root = Path(tmp)
        cpp = root / 'test.cpp'
        cpp.write_text(source, encoding='utf-8')
        exe = root / 'test.exe'
        result = subprocess.run([args.cxx, '/nologo', '/utf-8', '/std:c++17', '/EHsc', str(cpp), '/Fe:' + str(exe)], cwd=root, capture_output=True, text=True)
        if result.returncode:
            print(result.stdout + result.stderr)
            raise SystemExit(result.returncode)
        result = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True)
        print(result.stdout + result.stderr, end='')
        report = args.sketch.parent / 'tools/verification/shell-locale-0820.json'
        report.write_text(json.dumps({'passed': result.returncode == 0, 'realCode': ['Engine constructor', 'Engine::error', 'Engine::lex', 'Engine::help', 'Engine::printHelp', 'Engine::tr', 'Engine::out', 'Tab5Terminal locale adapter'], 'helpPages': len(help_entries), 'languages':14, 'result': result.stdout + result.stderr}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
