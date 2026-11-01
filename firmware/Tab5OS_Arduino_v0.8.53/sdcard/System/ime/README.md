CLAVIERS ET SAISIE — Tab5OS 0.8.18

Chaque pack de langue indique son clavier dans le champ "keyboard". Le réglage
"Clavier lié à la langue" applique cette association. Choisir directement une
disposition conserve ce choix jusqu'à la réactivation de l'association.

Chinois simplifié : saisir le pinyin sans tons (v pour ü). Une syllabe, un mot ou
une expression présents dans le dictionnaire donnent des candidats. Il n'y a
pas de segmentation automatique d'une longue phrase ni de prédiction apprise.
Séparer le texte en mots ou en syllabes si une lecture n'a pas de résultat.

Japonais : saisir en romaji. Hiragana et katakana sont disponibles (bouton あ/ア),
y compris syllabes composées, petits kana, consonnes doublées et n'. La conversion
en kanji porte sur une lecture exacte du dictionnaire SKK. Les verbes fléchis
se composent par segments ; cette version n'analyse pas une phrase complète et
n'utilise pas les règles d'okurigana de SKK. La saisie libre en kana reste possible.

Coréen : clavier standard à deux jeux de touches (두벌식 / 2-beolsik). La saisie
compose les 11 172 syllabes modernes, les voyelles et consonnes finales composées.
Retour arrière défait une frappe. La conversion en hanja n'est pas incluse.

La zone de composition reste distincte du document jusqu'à validation.
- Espace : proposer/cycler les candidats ; coréen : valider puis espace.
- Entrée : valider la composition ou le candidat choisi.
- 1 à 6 ou toucher un candidat : choisir le candidat affiché.
- Flèches gauche/droite : choisir ; Page précédente/suivante : changer de page.
- Retour arrière : retirer une frappe ; Échap : annuler la composition.
- Ctrl+Espace ou ABC : basculer entre saisie native et caractères latins.
Tous les candidats conservés dans le dictionnaire sont accessibles par pages.
Sans carte SD ou dictionnaire disponible, la conversion en kanji/hanzi est
indisponible ; la saisie phonétique seule ne remplace pas cette conversion.

Les polices CJK complètent la police principale. Seule la langue ou le clavier
utilisé est chargé ; deux répertoires sont nécessaires si la langue CJK et le
clavier CJK choisis diffèrent. Chaque fichier est limité à 6 Mio. Les polices
fournies couvrent les textes des packs et tous les candidats des dictionnaires,
ainsi que toutes les syllabes coréennes modernes. Un document externe contenant
des caractères rares absents de ces répertoires peut demander une autre police.

SOURCES ET LICENCES

Les fichiers .dat sont des index créés pour Tab5OS à partir de :
- Rime pinyin_simp, dérivé du PinyinIME Android, licence Apache 2.0.
  https://github.com/rime/rime-pinyin-simp
  Licence : pinyin-LICENSE.txt ; source exacte : sources/pinyin_simp.dict.yaml.gz
- SKK-JISYO.L, SKK Development Team et contributeurs, GPL version 2 ou ultérieure.
  https://github.com/skk-dev/dict
  Licence : SKK-COPYING.txt ; source exacte avec crédits : sources/SKK-JISYO.L.gz
  Les entrées à expressions exécutables, abréviations ou okurigana sont exclues ;
  aucune expression du dictionnaire n'est exécutée par Tab5OS.

Les polices cjk-zh.ttf, cjk-ja.ttf et cjk-ko.ttf sont des sous-ensembles renommés
"Tab5 CJK" de Noto Sans SC/JP/KR, poids 400, sans hints ni tables de composition
inutilisées. Noto : Google et Adobe, SIL Open Font License 1.1 (OFL.txt).
https://github.com/notofonts/noto-cjk
Les polices originales ne sont pas modifiées sur l'ordinateur de l'utilisateur.

Reconstruction depuis firmware/ : build/resources/build_cjk_assets.py
(Python + FontTools). Les adaptations ne nécessitent aucune connexion réseau
pendant l'utilisation sur le Tab5.
