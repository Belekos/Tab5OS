# Vérification 0.8.49

Le firmware complet a été construit avec GCC RISC-V et la chaîne épinglée. Les sources sont restées identiques pendant la compilation finale. Les preuves distinguent les tests natifs sur ordinateur, la compilation P4 et les essais physiques encore nécessaires.

| Mesure | Résultat |
| --- | ---: |
| Firmware OTA | [Tab5OS_v0.8.49.bin](firmware/Tab5OS_v0.8.49.bin) |
| Taille | 6 093 264 octets |
| Partition OTA | 6 553 600 octets |
| Marge | 460 336 octets |
| DATA + BSS internes au lien | 93 968 octets |
| Code IRAM | 98 882 octets |
| Sources vérifiées / copies Arduino | 2 035 / 1 923 |
| Objets de la construction complète | 1 425 |

La table de partitions est inchangée depuis la référence livrée 0.8.48. Les 23 archives de dépendances et Arduino CLI ont été revérifiés par SHA-256. Les copies de bibliothèques restent distinctes de l’installation SDK.

L’ELF final confirme le passage JPEG par le wrapper de cache, les fonctions IRAM, la consultation bornée de l’allocateur SDK authentifié, les hooks mémoire LVGL vers son TLSF natif et la nouvelle primitive Speaker `stopAndWait`. Une option de compilation ou un fichier patché sur disque n’est pas considéré seul comme une preuve de liaison.

La passe de diagnostic sur neuf unités enlève `-w` et active notamment les erreurs de retour, d’initialisation et d’usage après libération. Elle réussit avec **320 avertissements**, dont 316 dans le fichier principal. La compilation des sources 0.8.48 authentifiées avec les mêmes options retrouve les mêmes catégories et comptes. Les deux nouvelles unités sans historique n’émettent aucun avertissement. Cela ne signifie pas que les avertissements existants sont tous résolus.

Les preuves de cette construction se trouvent sous `tools/verification/build-0849` et dans `tools/verification/firmware-verification-0849.json`. Les rapports `*-current-0849.json` sont des enveloppes de publication qui référencent par empreinte leurs rapports d’exécution actuels inchangés ; ils ne représentent pas une nouvelle exécution.

Les essais couvrent l’admission mémoire, le TLSF LVGL réel, la synchronisation de cache contre le code natif SDK, le pilote audio avec le worker Speaker, les durées de vie vidéo et les non-régressions ciblées de stockage/réseau. Ils ne mesurent pas le bus PSRAM, les interruptions et le son de la tablette physique. Aucun résultat de disparition des flashs ou des claquements n’est annoncé.

Voir [RAPPORT_INGENIERIE_0.8.49.md](RAPPORT_INGENIERIE_0.8.49.md) et [TESTS_TAB5_0.8.49.md](TESTS_TAB5_0.8.49.md).
