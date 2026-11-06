# Keyboards and input - Tab5OS

Each language pack specifies its keyboard in the `keyboard` field. The
**Match keyboard to language** setting applies this association. Selecting a
layout directly keeps that choice until the association is enabled again.

**Simplified Chinese:** type pinyin without tones (`v` for `ü`). A syllable,
word or phrase present in the dictionary produces candidates. Long sentences
are not segmented automatically, and there is no learned prediction.
Split the text into words or syllables if a reading returns no results.

**Japanese:** type romaji. Hiragana and katakana are available (the あ/ア button),
including compound syllables, small kana, doubled consonants and `n'`.
Kanji conversion matches an exact reading in the SKK dictionary. Conjugated
verbs are composed in segments; this version does not analyze a whole sentence
or use SKK okurigana rules. Free kana input remains available.

**Korean:** the standard two-set keyboard (두벌식 / 2-beolsik). Input composes
all 11,172 modern syllables, including compound vowels and final consonants.
Backspace undoes one keystroke. Hanja conversion is not included.

The composition area remains separate from the document until you confirm it.

- Space: suggest or cycle through candidates; in Korean, commit and insert a space.
- Enter: commit the composition or selected candidate.
- 1 to 6, or tap a candidate: choose the displayed candidate.
- Left/Right arrows: select; Page Up/Down: change the page.
- Backspace: remove one keystroke; Escape: cancel the composition.
- Ctrl+Space or ABC: switch between native input and Latin characters.

All candidates retained in the dictionary are accessible through pages.
Without an SD card or an available dictionary, kanji/hanzi conversion is
unavailable; phonetic input alone does not replace that conversion.

CJK fonts supplement the main font. Only the active language or keyboard's
character set is loaded; two character sets are needed if the selected CJK
language and CJK keyboard differ. Each file is limited to 6 MiB. The supplied
fonts cover language-pack text and every dictionary candidate, as well as all
modern Korean syllables. An external document containing rare characters
absent from these character sets may require another font.

## Sources and licenses

The `.dat` files are indexes created for Tab5OS from:

- Rime pinyin_simp, derived from Android PinyinIME, Apache 2.0 license.
  https://github.com/rime/rime-pinyin-simp
  License: `pinyin-LICENSE.txt`; exact source: `sources/pinyin_simp.dict.yaml.gz`.
- SKK-JISYO.L, SKK Development Team and contributors, GPL version 2 or later.
  https://github.com/skk-dev/dict
  License: `SKK-COPYING.txt`; exact source with credits: `sources/SKK-JISYO.L.gz`.
  Entries containing executable expressions, abbreviations or okurigana are
  excluded; Tab5OS never executes dictionary expressions.

The `cjk-zh.ttf`, `cjk-ja.ttf` and `cjk-ko.ttf` fonts are renamed "Tab5 CJK"
subsets of Noto Sans SC/JP/KR, weight 400, without hints or unused shaping tables.
Noto: Google and Adobe, SIL Open Font License 1.1 (`OFL.txt`).
https://github.com/notofonts/noto-cjk
The original fonts on the user's computer are not modified.

Rebuild from `firmware/` with `build/resources/build_cjk_assets.py`
(Python + FontTools). These adaptations do not require a network connection
while using the Tab5.
