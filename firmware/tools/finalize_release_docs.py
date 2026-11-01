"""Publish documentation evidence only after the delivered firmware is verified.

Run after verify_final.py and after copying the OTA BIN and SHA256.txt:
    python -X utf8 tools/finalize_release_docs.py
Use --check to validate and preview the changes without writing anything.
The release root is always the parent of this script's tools directory.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

from verify_features_0828 import collect_evidence, read_selected, safe_path


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def replace_known(text, old, new, name):
    if old in text:
        require(text.count(old) == 1, f"Ambiguous documentation anchor: {name}")
        return text.replace(old, new, 1)
    require(new in text, f"Documentation anchor changed: {name}")
    return text


def checked_keyboard_shortcuts(sketch, reports):
    report = read_selected(sketch.parent, "keyboard-shortcuts-0819.json")
    required = {"fr-FR", "de-DE", "ru-RU", "ar", "ko-KR", "ja-JP", "zh-CN"}
    require(report.get("passed") is True and required <= set(report.get("layouts", [])),
            "Keyboard shortcut tests are missing or failed")
    match = re.search(r"PASS (\d+) real decoder checks", report.get("output", ""))
    require(match and int(match[1]) >= 206, "Incomplete keyboard decoder checks")
    require({"WriterKeys.cpp", "WriterKeys.h"} <= report.get("sources", {}).keys(),
            "Keyboard decoder source evidence is incomplete")
    for name, expected in report["sources"].items():
        path = (sketch / name).resolve()
        require(path.is_relative_to(sketch.resolve()), "Unsafe keyboard source path")
        require(digest(path) == expected, f"Keyboard decoder changed after its tests: {name}")
    require(digest(sketch / "sdcard/System/keyboards.json") == report.get("keyboardSha256"),
            "Keyboard layouts changed after shortcut tests")
    return report, int(match[1])


def checked_evidence(release, qa):
    version = re.fullmatch(r"Tab5OS_Ecriture_v(\d+\.\d+\.\d+)", release.name)
    require(version, "Unexpected release directory name")
    version = version[1]
    build = read_json(qa / "build-result.json")
    require(build.get("mode") == "build" and type(build.get("exitCode")) is int
            and build["exitCode"] == 0, "The complete build has not succeeded")
    verification = read_json(qa / "firmware-verification.json")
    stability = read_json(qa / "artifact-stability.json")
    require(verification.get("version") == version, "Firmware report is for another release")
    require(verification.get("build") == build, "Firmware report is for another build invocation")
    for key in ("sourcesUnchanged", "generatedSketchMatches", "imageChecksumValid",
                "imageValidationHashValid", "elfEmbeddedShaMatches"):
        require(verification.get(key) is True, f"Missing successful firmware check: {key}")
    require(verification.get("imageChip") == "ESP32-P4", "Wrong image chip")
    require(verification.get("sourceCopyMismatches") == [], "Arduino source copies differ")
    require(verification.get("physicalDeviceTested") is False,
            "Review hardware-test wording before finalizing these documents")
    require(stability.get("passed") is True and stability.get("artifactsStable") is True,
            "Artifact stability verification failed or is incomplete")
    require(stability.get("before") and stability["before"] == stability.get("after"),
            "Artifact snapshots differ or are empty")
    checks = {item.get("script"): item.get("exitCode") for item in stability.get("checks", [])}
    require(checks.get("verify_build.py") == 0 and checks.get("verify_archives.py") == 0,
            "Firmware and archive verification must both succeed")
    for name, expected in verification.get("evidenceSha256", {}).items():
        require(Path(name).name == name, "Unsafe evidence path")
        require(digest(qa / name) == expected, f"Verification evidence changed: {name}")
    require(set(verification.get("evidenceSha256", {})) >= {
        "build.log", "compile-args.json", "source-manifest-start.json", "source-manifest-final.json"},
        "Incomplete firmware evidence manifest")
    sketch = release / f"Tab5OS_Arduino_v{version}_Ecriture"
    manifest = read_json(qa / "source-manifest-final.json")
    files = manifest.get("files", [])
    require(files and len(files) == verification.get("sourceFilesChecked"), "Invalid source manifest")
    actual_paths = {path.relative_to(sketch).as_posix() for path in sketch.rglob("*")
                    if path.is_file() and path.relative_to(sketch).parts[0] != "sdcard"}
    require(actual_paths == {item["path"] for item in files}, "Compiled source file set changed")
    for item in files:
        path = (sketch / item["path"]).resolve()
        require(path.is_relative_to(sketch.resolve()), "Unsafe source-manifest path")
        require(path.stat().st_size == item["bytes"] and digest(path) == item["sha256"],
                f"Compiled source changed after verification: {item['path']}")
    image = release / "firmware" / f"Tab5OS_v{version}.bin"
    artifact = verification.get("artifacts", {}).get(".bin", {})
    size, sha = image.stat().st_size, digest(image)
    require(size > 0 and size == artifact.get("bytes") == verification.get("firmwareBytes")
            and sha == artifact.get("sha256"), "Delivered BIN does not match the verified build")
    require(any(item.get("path") == artifact.get("file") and item.get("bytes") == size
                and item.get("sha256") == sha for item in stability["after"]),
            "Verified BIN is absent from the stable artifact snapshot")
    partition = verification.get("otaPartitionBytes", 0)
    require(type(partition) is int and size <= partition and
            partition - size == verification.get("otaFreeBytes"), "Invalid OTA partition accounting")
    checksum = (release / "firmware" / "SHA256.txt").read_text(encoding="utf-8-sig")
    matching = []
    for line in checksum.splitlines():
        match = re.fullmatch(r"\s*([0-9a-fA-F]{64})\s+\*?(.+?)\s*", line)
        if match and match[2].replace("\\", "/").split("/")[-1] == image.name:
            matching.append(match[1].lower())
    require(matching == [sha], "SHA256.txt needs exactly one correct checksum for the delivered BIN")
    require(read_json(qa / "toolchain-versions.json") == verification.get("toolVersions"),
            "Toolchain version evidence does not match the firmware report")

    reports = release / "tools" / "verification"
    languages = read_selected(release, "languages-0828.json")
    sd = sketch / "sdcard"
    keys = set(read_json(sd / "System/languages/en-US.json")["translations"])
    require(len(keys) == languages.get("referenceEntries"), "Language reference/audit mismatch")
    expected_locales = {"en-US", "fr-FR", "de-DE", "es-ES", "it-IT", "nl-NL", "pt-BR",
                        "pt-PT", "id-ID", "ru-RU", "ar", "zh-CN", "ja-JP", "ko-KR"}
    packs = languages.get("packs", {})
    require(set(packs) == expected_locales, "Expected fourteen audited language packs")
    for locale, audit in packs.items():
        require(audit.get("missingKeys") == 0 and audit.get("coveragePercent") == 100.0,
                f"Incomplete language: {locale}")
        for field in ("formatErrors", "lineBreakErrors", "untranslatedLongText", "unsupportedGlyphs"):
            require(audit.get(field) == [], f"Language audit failure: {locale}/{field}")
        path = sd / "System/languages" / (locale + ".json")
        require(path.stat().st_size == audit.get("bytes") and digest(path) == audit.get("sha256"),
                f"Pack changed after audit: {locale}")
        pack = read_json(path)
        require(keys <= pack["translations"].keys(), f"Missing current keys: {locale}")
        require(pack.get("keyboard") == locale and audit.get("keyboardAvailable") is True,
                f"Missing associated keyboard: {locale}")
    resources = read_selected(release, "resources-0828.json")
    require(resources.get("result") == "PASS" and resources.get("version") == version and
            resources.get("payload_matches_sd") is True, "Resource validation is missing or failed")
    factory_root = sd / f"Factory/Tab5OS-{version}"
    factory = read_json(factory_root / "manifest.json")
    require(factory.get("version") == version and
            len(factory["files"]) == resources.get("factory_resources"), "Factory count mismatch")
    for name, expected in factory["files"].items():
        path = (sd / name).resolve()
        require(path.is_relative_to(sd.resolve()), "Unsafe Factory resource path")
        require(digest(path) == expected and digest(factory_root / name) == expected,
                f"Factory resource changed after audit: {name}")
    ime = read_selected(release, "ime-0819.json")
    require(ime.get("passed") is True and ime.get("sources") and ime.get("assets"),
            "IME tests or their source/resource manifests are missing")
    for base, manifest_name in ((sketch, "sources"), (sd, "assets")):
        for name, expected in ime[manifest_name].items():
            # Historical native IME evidence stays unchanged. Current language
            # packs are checked against their new complete audit above.
            if manifest_name == "assets" and name.startswith("System/languages/"):
                require(name.split("/")[-1].removesuffix(".json") in packs,
                        "Historical language has no current audit")
                continue
            path = (base / name).resolve()
            require(path.is_relative_to(base.resolve()), "Unsafe IME manifest path")
            require(digest(path) == expected, f"IME {manifest_name} changed after audit: {name}")
    cjk = read_json(reports / "cjk-assets-0818.json")
    asset_hashes = set(ime["assets"].values())
    factory_hashes = set(factory["files"].values())
    for language in ("zh", "ja", "ko"):
        expected = cjk.get("fonts", {}).get(language, {}).get("sha256")
        require(expected in asset_hashes and expected in factory_hashes,
                f"CJK font absent from verified assets: {language}")
    for language in ("zh", "ja"):
        expected = cjk.get("dictionaries", {}).get(language, {}).get("sha256")
        require(expected in asset_hashes and expected in factory_hashes,
                f"IME dictionary absent from verified assets: {language}")
    shortcuts, shortcut_checks = checked_keyboard_shortcuts(sketch, reports)
    usb = read_selected(release, "gamepad-0822.json")
    require(usb.get("passed") is True and usb.get("current", {}).get("exitCode") == 0,
            "USB gamepad decoder tests failed")
    theme = read_json(reports / "theme-0818.json")
    require(theme.get("p4SyntaxExitCode") == 0 and theme.get("nativeCases"),
            "Theme tests are missing or failed")
    for path in (reports / "theme-0818.log", reports / "network-native-0818.log",
                 reports / "home-shell-0818.log", release / "tools/file-selection-tests/results.txt"):
        require("PASS" in path.read_text(encoding="utf-8"), f"Missing successful test: {path.name}")
    features = read_json(reports / "features-0828.json")
    require(features.get("version") == version and features.get("passed") is True and
            features.get("physicalDeviceTested") is False and
            features.get("sources") and features.get("reports"),
            "Current release regression evidence is missing")
    current_features = collect_evidence(release)
    require(current_features["passed"], "Current regression checks failed: " +
            "; ".join(current_features["errors"]))
    require(features["sources"] == current_features["sources"] and
            features["reports"] == current_features["reports"] and
            features.get("verifierSha256") == current_features["verifierSha256"] and
            features.get("ruleSetSha256") == current_features["ruleSetSha256"] and
            features.get("additionalContractsSha256") == current_features["additionalContractsSha256"] and            features.get("additionalContractFilesSha256") == current_features["additionalContractFilesSha256"] and
            features.get("dependencies") == current_features["dependencies"] and
            features.get("checks") == current_features["checks"],
            "The feature manifest is stale or incomplete; rerun verify_features_0828.py")
    for name, expected in features["sources"].items():
        path = (sketch / name).resolve()
        require(path.is_relative_to(sketch.resolve()) and digest(path) == expected,
                "Source changed after current regression checks: " + name)
    for name, expected in features["reports"].items():
        require(digest(safe_path(reports, name)) == expected,
                "Current regression report changed: " + name)
    return dict(version=version, image=image, bytes=size, sha=sha, partition=partition,
                verification=verification, languages=languages, resources=resources,
                ime=ime, cjk=cjk, usb=usb, theme=theme, keyboardShortcuts=shortcuts,
                keyboardShortcutChecks=shortcut_checks, features=features,
                directory=read_selected(release, "directory-picker-0822.json"),
                ui=read_selected(release, "ui-controls-0822.json"),
                calculator=read_selected(release, "calculator-0822.json"),
                terminal=read_selected(release, "terminal-0822.json"),
                reader=read_selected(release, "reader-layout-0822.json"),
                launchRadio=read_selected(release, "launch-radio-0822.json"))


def block(text, name, replacement, plain=False):
    first, last = (("[" + name + "]", "[/" + name + "]") if plain else
                   ("<!-- " + name + " -->", "<!-- /" + name + " -->"))
    require(text.count(first) == text.count(last) == 1, "Documentation block changed: " + name)
    start, end = text.index(first) + len(first), text.index(last)
    require(start < end, "Invalid documentation block order: " + name)
    return text[:start] + "\n" + replacement.strip() + "\n" + text[end:]


def render_documents(release, evidence):
    e = evidence
    version = e["version"]
    count, packs = e["languages"]["referenceEntries"], len(e["languages"]["packs"])
    factory, embedded = e["resources"]["factory_resources"], e["resources"]["embedded_resources"]
    native_checks = {name: int(re.search(r"PASS (\d+) checks", e[name]["output"])[1])
                     for name in ("directory", "ui", "calculator")}
    names = ["LISEZ_MOI.md", "VERIFICATION.md", "COMPILATION.md", f"NOUVEAUTES_v{version}.md",
             "READ_FIRST_EN.txt", "BIBLIOTHEQUES.md"]
    docs = {name: (release / name).read_text(encoding="utf-8") for name in names}
    vlink = "[firmware-verification.json](tools/verification/firmware-verification.json)"
    summary = f"**Compilation complète et contrôles de l’image {version} réussis.** Preuve : {vlink}."
    for name in names:
        if name.endswith(".md"):
            docs[name] = block(docs[name], "build-status", summary)
    docs["BIBLIOTHEQUES.md"] = docs["BIBLIOTHEQUES.md"].replace(
        "Les preuves de la nouvelle compilation seront consignées après sa validation.",
        "Les versions exécutées sont consignées dans [toolchain-versions.json](tools/verification/toolchain-versions.json).")
    news = f"NOUVEAUTES_v{version}.md"
    docs[news] = docs[news].replace("Quatorze packs sont prévus", "Quatorze packs sont livrés")
    resources = (f"Les {packs} packs couvrent les {count} clés de référence contrôlées. "
        f"Factory contient {factory} ressources officielles ; le secours firmware en contient {embedded}. "
        "Les grandes polices et les dictionnaires de saisie sont distribués sur SD. "
        "Preuves : [languages-0828.json](tools/verification/languages-0828.json), "
        "[ime-0828.json](tools/verification/ime-0828.json), "
        "[resources-0828.json](tools/verification/resources-0828.json).")
    docs[news] = block(docs[news], "resource-summary", resources)
    verified = (summary + "\n\n"
        "Les sources compilées et leurs copies Arduino correspondent. Les artefacts sont restés stables pendant les contrôles ; "
        "l’image ESP32-P4 passe le checksum et le hash de validation, et son empreinte ELF intégrée correspond à l’ELF compilé.\n\n"
        "| Contrôle | Valeur vérifiée |\n|---|---|\n"
        f"| Image OTA livrée | `firmware/{e['image'].name}` |\n"
        f"| Taille de l’image | {e['bytes']} octets |\n"
        f"| Partition OTA | {e['partition']} octets |\n"
        f"| Marge | {e['partition']-e['bytes']} octets |\n"
        f"| SHA-256 du BIN | `{e['sha']}` |\n\n"
        "Autres preuves : [build-result.json](tools/verification/build-result.json), "
        "[artifact-stability.json](tools/verification/artifact-stability.json), "
        "[image-info.log](tools/verification/image-info.log), "
        "[toolchain-versions.json](tools/verification/toolchain-versions.json). "
        "Le paquet vérifie aussi les fichiers ELF et map livrés dans `firmware/debug`. "
        "Aucun essai physique ni téléversement n’a été réalisé.")
    docs["VERIFICATION.md"] = block(docs["VERIFICATION.md"], "build-status", verified)
    rows = []
    current_count = sum(not row["historical"] for row in e["features"]["checks"])
    history_count = len(e["features"]["checks"]) - current_count
    for row in e["features"]["checks"]:
        name = row["report"]
        scope = ("Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés."
                 if row["historical"] else "Exécution 0.8.28 ; sources et banc de test identifiés par empreinte.")
        rows.append((name.removesuffix(".json"), scope, "tools/verification/" + name))
    table = (f"{current_count} rapports actuels et {history_count} preuves historiques réutilisées après contrôle des empreintes. "
             "Ces nombres dénombrent des rapports, pas des essais physiques.\n\n"
             "| Partie | Résultat et portée | Preuve |\n|---|---|---|\n" + "\n".join(
             f"| {title} | {result} | [{Path(path).name}]({path}) |" for title, result, path in rows))
    table += "\n\n" + resources
    table += ("\n\nLes rapports historiques conservent leurs dates, versions, résultats et limites. "
              "Le renommage du dossier de sketch permet seulement de retrouver les mêmes fichiers : "
              "leurs empreintes doivent rester strictement identiques. Les preuves concernant une source modifiée sont refusées. "
              "Les anciens tests Pentest natifs ne servent pas de preuve à l’application ELF : les nouveaux rapports vérifient son code, "
              "le panneau OS et leurs frontières. Le gate [features-0828.json](tools/verification/features-0828.json) "
              "consigne chaque preuve réellement acceptée et chaque remplacement. La compilation complète porte sur la totalité "
              "de la livraison. Aucun essai physique, mesure SRAM sur Tab5 ni téléversement n’est revendiqué.")
    docs["VERIFICATION.md"] = block(docs["VERIFICATION.md"], "test-results", table)
    docs["READ_FIRST_EN.txt"] = block(docs["READ_FIRST_EN.txt"], "BUILD_STATUS",
        f"The complete {version} build succeeded with exit code 0. Source identity, the ESP32-P4 image, embedded ELF hash and artifact stability passed verification. See tools/verification/firmware-verification.json.", True)
    docs["READ_FIRST_EN.txt"] = block(docs["READ_FIRST_EN.txt"], "RESOURCE_SUMMARY",
        f"Verified: {packs} language packs and {count} reference keys; {factory} Factory resources and {embedded} firmware recovery resources. Delivered OTA image: {e['bytes']} bytes. SHA-256: {e['sha']}.", True)
    for name, text in docs.items():
        require("\ufffd" not in text, f"Invalid Unicode in {name}")
        require(not re.search(r"compilation[^.\n]*en attente|build[^.\n]*(?:pending|in progress)", text, re.I),
                f"Stale pending-build claim in {name}")
    return docs


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qa-dir", type=Path, default=Path("C:/workspace/tab5-qa-0828-clang"))
    parser.add_argument("--check", action="store_true", help="validate and render without writing")
    args = parser.parse_args(argv)
    release = Path(__file__).resolve().parent.parent
    evidence = checked_evidence(release, args.qa_dir)
    docs = render_documents(release, evidence)
    # Gather every proof first: no documentation is changed if a required file is absent.
    proof_names = ["firmware-verification.json", "build-result.json", "artifact-stability.json",
                   "toolchain-versions.json", "image-info.log", "build.log", "compile-args.json",
                   "source-manifest-start.json", "source-manifest-final.json"]
    proofs = {name: (args.qa_dir / name).read_bytes() for name in proof_names}
    if not args.check:
        target = release / "tools/verification"
        for name, data in proofs.items():
            (target / name).write_bytes(data)
        for name, text in docs.items():
            (release / name).write_text(text.rstrip() + "\n", encoding="utf-8")
    print(json.dumps({"mode": "check" if args.check else "written", "version": evidence["version"],
                      "firmwareBytes": evidence["bytes"], "firmwareSha256": evidence["sha"],
                      "documents": list(docs), "proofs": proof_names,
                      "physicalDeviceTested": False}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, FileNotFoundError) as error:
        print(f"REFUSED: {error}", file=sys.stderr)
        raise SystemExit(1)
