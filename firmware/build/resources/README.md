# Resources and PC utilities

These scripts are independent of compilation. Generated headers required by the firmware are already present in the sketch. Run the following commands from `firmware/`, using Python 3.10 or later (`python3` on Linux).

## Embedded resources

After modifying Python modules or translations in the `sdcard` directory, these commands regenerate the corresponding headers:

```powershell
python build/resources/generate_locale_fallback.py Tab5OS_Arduino_v0.8.53
python build/resources/generate_system_resources.py Tab5OS_Arduino_v0.8.53
```

They write into the selected sketch. They do not embed fonts, ELF applications or encyclopedias in the firmware.

## Offline city search

The generator takes a separately obtained GeoNames `cities1000.zip` archive and produces the binary index and a JSON report:

```powershell
python build/resources/build_gazetteer.py C:/Sources/cities1000.zip C:/Tab5SD/System/maps/gazetteer/cities.t5geo
```

Keep the GeoNames attribution and CC BY 4.0 license with the data. `--fold-header PATH` also regenerates the C++ normalization table; reserve it for coordinated changes to the generator and native reader. The script downloads no data.

## CJK dictionaries and fonts

The source files `pinyin_simp.dict.yaml` and `SKK-JISYO.L` must be supplied in the same directory. Keep their `pinyin-LICENSE.txt` and `SKK-COPYING.txt` notices; they are copied when present.

```powershell
python build/resources/build_cjk_assets.py --sketch Tab5OS_Arduino_v0.8.53 --sources C:/Sources/CJK
```

The command writes dictionaries to `sdcard/System/ime` and a report containing input hashes. It preserves `keyboards.json`. To rebuild fonts as well, add `--fonts` and install `fonttools`. You must then supply `NotoSansSC.ttf`, `NotoSansJP.ttf`, `NotoSansKR.ttf`, their OFL license, the main `sdcard/System/font.ttf` font and the sketch's language packs. Not all of these large files are included in the repository.

## Wikipedia on a FAT32 card

```powershell
python build/resources/preparer_wikipedia.py C:/Sources/wikipedia_fr_all_nopic_2026-05.zim E:/
```

Replace the filename with the Kiwix archive actually downloaded and `E:/` with the card root. The script copies the archive in 64 MiB parts, verifies their integrity and resumes interrupted copies. It can also write into a staging directory on the PC.

## Recovering a vault file on a PC

Install the Python `cryptography` package, then run:

```powershell
python build/resources/dechiffrer_coffre.py C:/Sauvegarde/document.t5v C:/Recuperation/document.txt
```

For a T5VAULT2 vault, add `--cle C:/Sauvegarde/secours.t5key`. The password is requested without displaying it. The source file is preserved and an existing destination is never overwritten.
