# Compilation et provenance — 0.8.45

La référence est l'archive livrée 0.8.44, conservée intacte : SHA-256 `b5fdc983e51e17568d700b9ae1359e902fa61f43c86505a775e650f10cab16aa`. Le manifeste extrait et authentifié contient 2 388 fichiers du sketch et 1 954 preuves historiques. Les anciennes preuves ne sont pas requalifiées en nouveaux essais.

La reconstruction utilise Arduino M5Stack 3.3.9 / ESP-IDF 5.5.4, GCC RISC-V 14.2.0, M5Unified 0.2.23, M5GFX 0.2.30, LVGL 9.6.0, ArduinoJson 7.4.3 et le tag ESP8266Audio 2.4.2 (métadonnée amont 2.4.1). Les 23 archives et Arduino CLI ont été revérifiés par empreinte. L'ABI, les options d'optimisation normales, la fréquence compilée et les partitions restent celles de la référence.

## Dépendance FS corrigée

La bibliothèque FS du SDK est préparée dans une copie séparée, comme les copies LVGL et M5GFX existantes. `prepare_fs_errno.py` exige la source VFS exacte, n'altère jamais l'installation du SDK et refuse une copie isolée inattendue. Le script final est lié à son empreinte, aux fichiers originaux et au résultat patché. La sélection de cette copie est explicite dans les trois points d'entrée de compilation : Linux/GCC, PowerShell et Clang Windows.

La vérification du firmware contrôle les dépendances réellement émises par le compilateur pour `FS.cpp` et `vfs_api.cpp`, les empreintes de leurs objets, l'absence de dépendances restantes vers les anciens en-têtes FS du SDK et l'origine unique des méthodes VFS liées. Une source patchée présente sur le disque ne suffit pas à valider la livraison.

L'analyse du SDK et les matrices d'erreur ont conduit à corriger avant livraison la priorité d'errno entre `fopen`, `stat` et `opendir`, notamment pour ne pas transformer EIO/ENOMEM en absence de fichier. Les sources et objets FS d'une compilation intermédiaire ont été invalidés. Le build final reconstruit tous les objets du sketch ainsi que FS à partir de la version figée, dans la même invocation que le nettoyage et la préparation des copies.

## Contrôle des avertissements

La commande Arduino normale contient `-w`. Son succès ne constitue donc pas une preuve d'absence d'avertissements. Un passage séparé sur les unités critiques retire cette suppression et active `-Wall`, `-Wextra` et les diagnostics bloquants de retour de fonction, utilisation de valeur non initialisée et utilisation après libération. Il conserve les autres choix du SDK et n'est pas le binaire livré. Les avertissements restent enregistrés ; ce passage ne démontre pas l'absence de défaut de durée de vie ou de concurrence.

## Périmètre de validation

La compilation finale doit être terminée et ses sources restées inchangées pour qu'une image soit livrable. Le vérificateur contrôle ensuite l'image ESP32-P4, ELF32/RISC-V/ABI, checksum et hash, les copies Arduino, les backports réellement liés, la table de partitions, la marge OTA et les sections mémoire. Le paquet impose les rapports courants des sources modifiées et la conservation des autres fichiers couverts par la référence.

Factory contient 180 ressources et le secours intégré 40 petits fichiers. Les 14 packs de langue et leur couverture/format/glyphes sont vérifiés ; aucune entrée ni métadonnée existante n'est modifiée. La nouvelle utilisation du libellé de recherche réemploie une traduction déjà présente. Les nouveaux formats techniques de l'affichage, du stockage et du diagnostic réseau sont uniquement série.

Seul le parcours Linux/GCC est exécuté pour cette livraison. Les scripts Windows ont été adaptés et relus ; le script Python Clang passe le contrôle de syntaxe. Aucun compilateur Windows ni PowerShell n'a été exécuté dans cet environnement. Aucun essai matériel Tab5 n'est annoncé.

## Résultat final

Compilation complète réussie, 1 423 objets ; 563 objets du sketch et les objets/archive FS ont été reconstruits après invalidation de la compilation intermédiaire. Le passage final utilise le helper FS définitif après normalisation de ses clés de manifeste pour Windows ; ses octets VFS restent identiques au résultat relu. Sources avant/après inchangées, copies Arduino et provenance des objets liées vérifiées.

| Mesure du firmware | 0.8.44 | 0.8.45 |
| --- | ---: | ---: |
| Image OTA, octets | 6 065 728 | 6 067 488 |
| Marge OTA, octets | 487 872 | 486 112 |
| Données internes statiques + BSS, octets | 93 268 | 93 268 |
| IRAM texte, octets | 98 494 | 98 494 |
| TCM texte / données, octets | 794 / 64 | 794 / 64 |
| RTC noinit, octets | 1 200 | 1 200 |

SHA-256 du BIN : `0b732239e9f63673a0d6afd67339206b8891f2f699e1e0dc9c26f7e0b940afbc`. Partitions inchangées. Aucun chiffre de tas libre réel n'est déduit de cette comparaison statique.

Le contrôle séparé de neuf unités critiques relève **343 avertissements**, exactement **343 dans les neuf unités de référence 0.8.44**, reconstruites avec les mêmes diagnostics et des sources/génération Arduino authentifiées. Chaque compte par catégorie et par unité est identique : 273 dépréciations, 18 initialisations partielles d'agrégats, 17 fonctions et 7 variables inutilisées, 9 indentations ambiguës, 7 conversions d'énumérations dépréciées, 6 avertissements de suffixe dans un en-tête SDK, 2 préprocesseur, 2 `class-memaccess`, 1 parenthésage et 1 troncature de format. Aucun diagnostic sélectionné bloquant ne s'est déclenché. Ce résultat n'est pas « sans avertissements ».

Les cas pertinents pour l'intégrité mémoire sont relus : les deux objets remis à zéro contiennent des champs numériques/énumérations/tableaux de caractères et aucune propriété String/allocation ; leurs déclarations sont identiques à la référence. `formatFileTime` reste identique avec un `snprintf` borné à 32 octets ; l'avertissement vise des valeurs entières théoriquement extrêmes, les dates FAT ordinaires sont bornées. Une anomalie de date pourrait encore tronquer un libellé. Ces points préexistants ne sont pas modifiés après gel pour masquer un avertissement. Les logs complets et la comparaison restent joints aux preuves de compilation.

Les rapports courants couvrent les douze sources de production modifiées. Le manifeste conserve aussi le petit rapport Maps P4 non compilé déjà présent dans le sketch au gel ; il n'est pas exclu artificiellement des empreintes. Le helper actif ancre désormais ses chemins de rapports sur la racine de livraison, indépendamment du répertoire courant. Les nouveaux rapports P4 sont relancés à leur emplacement correct.

Une compilation intermédiaire invalidée ne sert ni à l'image livrée ni à ces mesures. Aucun périphérique physique n'est déclaré testé.
