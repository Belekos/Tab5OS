# Ressources et utilitaires PC

Ces scripts sont indépendants de la compilation. Les en-têtes générés nécessaires au firmware sont déjà présents dans le sketch. Exécuter les commandes suivantes depuis `firmware/`, avec Python 3.10 ou plus récent (`python3` sous Linux).

## Ressources intégrées

Après modification des modules Python ou des traductions du dossier `sdcard`, ces commandes régénèrent les en-têtes correspondants :

```powershell
python build/resources/generate_locale_fallback.py Tab5OS_Arduino_v0.8.53
python build/resources/generate_system_resources.py Tab5OS_Arduino_v0.8.53
```

Elles écrivent dans le sketch sélectionné. Elles n'incorporent ni polices, ni applications ELF, ni encyclopédies dans le firmware.

## Recherche de villes hors ligne

Le générateur reçoit une archive GeoNames `cities1000.zip` obtenue séparément et produit l'index binaire ainsi qu'un rapport JSON :

```powershell
python build/resources/build_gazetteer.py C:/Sources/cities1000.zip C:/Tab5SD/System/maps/gazetteer/cities.t5geo
```

Conserver l'attribution et la licence CC BY 4.0 de GeoNames avec les données. L'option `--fold-header CHEMIN` régénère aussi la table de normalisation C++ ; elle est réservée aux modifications coordonnées du générateur et du lecteur natif. Le script ne télécharge aucune donnée.

## Dictionnaires et polices CJK

Les sources `pinyin_simp.dict.yaml` et `SKK-JISYO.L` doivent être fournies dans un même dossier. Conserver leurs notices `pinyin-LICENSE.txt` et `SKK-COPYING.txt` ; elles sont copiées lorsqu'elles sont présentes.

```powershell
python build/resources/build_cjk_assets.py --sketch Tab5OS_Arduino_v0.8.53 --sources C:/Sources/CJK
```

La commande écrit les dictionnaires dans `sdcard/System/ime` et un rapport avec les empreintes des entrées. Elle conserve `keyboards.json`. Pour reconstruire également les polices, ajouter `--fonts` et installer `fonttools`. Il faut alors fournir `NotoSansSC.ttf`, `NotoSansJP.ttf`, `NotoSansKR.ttf`, leur licence OFL, ainsi que la police principale `sdcard/System/font.ttf` et les packs de langue du sketch. Ces fichiers volumineux ne sont pas tous inclus dans le dépôt.

## Wikipédia sur une carte FAT32

```powershell
python build/resources/preparer_wikipedia.py C:/Sources/wikipedia_fr_all_nopic_2026-05.zim E:/
```

Remplacer le nom par celui de l'archive Kiwix réellement téléchargée et `E:/` par la racine de la carte. Le script copie l'archive en parties de 64 Mio, vérifie leur intégrité et reprend une copie interrompue. Il peut aussi écrire dans un dossier de préparation sur PC.

## Récupérer un fichier du coffre sur PC

Installer le paquet Python `cryptography`, puis :

```powershell
python build/resources/dechiffrer_coffre.py C:/Sauvegarde/document.t5v C:/Recuperation/document.txt
```

Pour un coffre T5VAULT2, ajouter `--cle C:/Sauvegarde/secours.t5key`. Le mot de passe est demandé sans affichage. Le fichier source est conservé et une destination existante n'est jamais écrasée.
