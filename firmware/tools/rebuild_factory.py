"""Rebuild official SD resources and hashes without including user files."""
import argparse, ast, hashlib, json, shutil
from pathlib import Path

# Renamed official resources for 0.8.38. This only updates the shipped manifest;
# it never moves or renames files on a user's card.
RESOURCE_RENAMES = {
    "MicroPython/bibliotheques.py": "MicroPython/libraries.py",
    "MicroPython/exemple.py": "MicroPython/example.py",
    "MicroPython/importer_cartes.py": "MicroPython/import_maps.py",
    "MicroPython/importer_radios.py": "MicroPython/import_radios.py",
    "MicroPython/mettre_a_jour_sd.py": "MicroPython/update_sd.py",
    "MicroPython/creer_theme.py": "MicroPython/create_theme.py",
    "Games/GB/LISEZ-MOI.txt": "Games/GB/README.txt",
    "Images/LISEZ-MOI.txt": "Images/README.txt",
    "System/maps/offline/LISEZ-MOI.txt": "System/maps/offline/README.txt",
    "System/CLAVIERS.txt": "System/KEYBOARDS.txt",
    "Packs/Radios/LISEZ_MOI.txt": "Packs/Radios/README.txt",
}

def prepare_manifest(sd, version):
    sd = sd.resolve()
    module = sd / "MicroPython/lib/tab5factory.py"
    values = {}
    for node in ast.parse(module.read_text(encoding="utf-8")).body:
        if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
            values[node.targets[0].id] = ast.literal_eval(node.value)
    old = values["VERSION"]
    previous = values.get("PREVIOUS", {})
    old_factory = sd / "Factory" / ("Tab5OS-" + old)
    if old != version:
        old_manifest = json.loads((old_factory / "manifest.json").read_text(encoding="utf-8"))
        for name, digest in old_manifest["files"].items():
            # Retain the old digest under its migrated logical destination.
            # The historical manifest itself remains byte-identical.
            if name.startswith("M5Writer/"):
                name = "System/" + name[len("M5Writer/"):]
            previous[name] = sorted(set(previous.get(name, []) + [digest]))
    languages={p.relative_to(sd).as_posix() for p in (sd / 'System/languages').glob('*.json')}
    cjk_assets = {
        "System/fonts/cjk-zh.ttf", "System/fonts/cjk-ja.ttf", "System/fonts/cjk-ko.ttf",
        "System/ime/zh-pinyin.dat", "System/ime/ja-kana.dat",
        "System/ime/OFL.txt", "System/ime/pinyin-LICENSE.txt", "System/ime/SKK-COPYING.txt",
        "System/ime/sources/SKK-JISYO.L.gz", "System/ime/sources/pinyin_simp.dict.yaml.gz",
        "System/ime/README.md",
        "System/languages/zh-CN.json", "System/languages/ja-JP.json", "System/languages/ko-KR.json"
    }
    gazetteer = {"System/maps/gazetteer/README.txt", "System/maps/gazetteer/cities.json", "System/maps/gazetteer/cities.t5geo"}
    python_modules={p.relative_to(sd).as_posix() for p in (sd / 'MicroPython').rglob('*.py')}
    radio_packs={p.relative_to(sd).as_posix() for p in (sd / 'Packs/Radios').glob('*.json')}
    application_files={p.relative_to(sd).as_posix() for p in (sd / "Applications").rglob("*") if p.is_file() and (p.suffix.lower()==".elf" or p.name.endswith(".elf.launch.json"))}
    files = sorted({RESOURCE_RENAMES.get(name, name) for name in values["FILES"]} | application_files | languages | cjk_assets | python_modules | radio_packs | gazetteer | {
        "Images/README.txt",
        "Picture/README.txt",
        "Packs/Radios/README.txt",
        "Applications/Pentest.app.elf",
        "Applications/Pentest.app.elf.launch.json",
        "MicroPython/lib/tab5library.py", "MicroPython/lib/tab5libraryui.py",
        "System/languages/ar.json", "System/languages/ru-RU.json",
        "System/languages/id-ID.json"})
    content = ('"""Generated release resources. Updated with Factory/manifest.json."""\n'
               + "VERSION = " + repr(version) + "\n"
               + "FILES = " + repr(tuple(files)) + "\n"
               + "PREVIOUS = " + repr(previous) + "\n")
    module.write_text(content, encoding="utf-8", newline="\n")
    return files

def build(sd, version):
    sd = sd.resolve()
    files = prepare_manifest(sd, version)
    for name in files:
        source = (sd / name).resolve()
        if sd not in source.parents or not source.is_file():
            raise RuntimeError("Missing or unsafe resource: " + name)
    factory = sd / "Factory" / ("Tab5OS-" + version)
    # Build a fresh tree; the caller archives previous release resources.
    # Only the current distribution is rebuilt; user cards are never modified.
    factory.mkdir(parents=True, exist_ok=True)
    hashes = {}
    for name in files:
        data = (sd / name).read_bytes()
        target = factory / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        hashes[name] = hashlib.sha256(data).hexdigest()
    manifest = {"version": version, "files": hashes}
    (factory / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for name, digest in hashes.items():
        assert hashlib.sha256((factory / name).read_bytes()).hexdigest() == digest
        assert (factory / name).read_bytes() == (sd / name).read_bytes()
    print(json.dumps({"version":version,"resources":len(files),"verified":True}))
if __name__ == "__main__":
    p=argparse.ArgumentParser()
    p.add_argument("sd",type=Path)
    p.add_argument("--version",required=True)
    p.add_argument("--prepare-manifest",action="store_true",
                   help="Freeze embedded resource paths before all separate SD translations are finished; no Factory verification is claimed.")
    a=p.parse_args()
    if a.prepare_manifest:
        print(json.dumps({'version':a.version,'paths_prepared':len(prepare_manifest(a.sd,a.version)),'factory_built':False}))
    else:
        build(a.sd,a.version)
