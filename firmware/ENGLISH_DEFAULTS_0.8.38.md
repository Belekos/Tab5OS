# Noms par défaut en anglais — 0.8.38

Les noms de stockage créés par l’OS sont indépendants de la langue d’interface. Les libellés, dialogues et titres d’applications restent traduits. Les chemins et noms choisis par l’utilisateur sont conservés.

| Usage | Nouveau défaut |
| --- | --- |
| Dossier créé dans l’explorateur | `New folder` |
| Suffixe d’une copie | ` - copy` |
| Enregistrer un nouveau roman | `Novel.md` |
| Projet sans titre déduit du dossier | `Project` |
| Rapport de diagnostic | `report-…` |
| Catalogue de bibliothèques Python | `/MicroPython/Libraries` |
| Nouveaux projets Python | `/MicroPython/Projects` |
| Famille de carte générique | `other` |
| Catégories Python | `general`, `sensors`, `display`, `network`, `storage`, `motors`, `audio` |

Les scripts livrés portent désormais les noms `libraries.py`, `example.py`, `import_maps.py`, `import_radios.py`, `update_sd.py` et `create_theme.py`. Les notices SD `LISEZ-MOI.txt`/`LISEZ_MOI.txt` deviennent `README.txt` dans `Games/GB`, `Images`, `System/maps/offline` et `Packs/Radios`. `System/CLAVIERS.txt` devient `System/KEYBOARDS.txt`. Le contenu de ces onze fichiers est inchangé.

Les anciennes bibliothèques `/MicroPython/Bibliotheques` et les anciens projets `/MicroPython/Projets` restent accessibles sur place. La lecture des métadonnées reconnaît les anciens identifiants français sans écrire sur la carte. Un rattachement demandé par l’utilisateur conserve le chemin de son projet et refuse d’écraser un fichier différent. Les anciens noms de scripts restent reconnus pour les cartes déjà utilisées. Si les deux noms existent, le catalogue affiche la version anglaise ; l’ancien fichier reste accessible dans l’explorateur. Ce choix demande au plus cinq sondes de fichier, sans parcours supplémentaire de répertoire. Il n’y a pas de renommage ni de migration automatique de fichiers personnels.

Les valeurs déjà en anglais sont maintenues, notamment `Documents`, `Projects`, `Audio`, `Recordings`, `Video`, `Videos`, `Images`, `Pictures/Maps`, `System` et les noms techniques de cache, réglages et exports. Les intitulés français des exemples de documentation ne sont pas des noms créés automatiquement.

Vérification reproductible depuis la racine de la livraison :

```sh
python tools/english-defaults-tests-0838/run.py
```

L’option `--baseline /chemin/vers/Tab5OS_Ecriture_v0.8.37` vérifie aussi que les onze renommages conservent exactement les octets précédents. Les tests exécutent les modules Python réels, les transactions, les refus d’écrasement, les retours arrière après interruption, la compatibilité des anciens chemins et le catalogue C++ réel avec ASan/UBSan. Les adaptateurs disque/réseau/affichage sont simulés sur l’hôte ; ce rapport ne prétend pas à une mesure sur Tab5.
