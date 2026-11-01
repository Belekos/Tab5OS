# Compilation et limites de vérification 0.8.51

La référence fonctionnelle est l’archive 0.8.50 livrée, SHA-256
`ea13e38382767ee6e061d5a099b553f6a42461454b1a5bbd7bc653893a802c3b`.
Les mesures de sections de cette version sont conservées dans `build-baseline.json`.
Les rapports 0.8.50 et antérieurs restent historiques et ne sont pas des exécutions 0.8.51.

La chaîne réutilise les dépendances épinglées Arduino CLI 1.5.1, Arduino
M5Stack 3.3.9 / ESP-IDF 5.5.4, GCC RISC-V 14.2.0, M5Unified 0.2.23,
M5GFX 0.2.30 et LVGL 9.6.0. Les répertoires de compilation et les preuves
0.8.51 sont séparés. Les adaptations natives DSI, audio, FS, LVGL et
mémoire restent celles de 0.8.49, dont un contrôle binaire source porte sur
44 fichiers. Aucun SDK installé, pilote de communications ni partition
n’est modifié pour cette évolution de Dessin.

La compilation complète prend un manifeste avant et après son exécution.
Des sources modifiées pendant le travail invalident le résultat, même si
le compilateur réussit. La vérification du firmware contrôle ensuite
l’image ESP32-P4, son intégrité, le hash ELF embarqué, la limite des deux
partitions OTA, les copies Arduino et la provenance effective des unités
adaptées. Le placement IRAM/interne et les adaptateurs mémoire/cache
sont vérifiés dans l’ELF lié, pas simplement dans les options de commande.

Le fichier BIN livré séparément est une image applicative OTA, pas une
image fusionnée à écrire à l’adresse zéro. La table de partitions reste
strictement inchangée.

Les contrôles sur ordinateur ne mesurent ni le son analogique, ni les
transferts DSI/PSRAM/microSD, ni la latence tactile. Les relevés statiques
DATA/BSS ne constituent pas une mesure du tas libre en fonctionnement.
Aucun essai sur une Tab5 physique n’est revendiqué.

## Résultats exécutés

La compilation complète du 29 octobre 2026 a terminé avec le code de retour
0 en 6 min 07 s ; le manifeste est resté inchangé. Les vérifications finales
portent sur 2 038 entrées source, 1 926 copies Arduino et 1 426 objets.
L’image et l’ELF passent les contrôles d’intégrité, de provenance native
DSI/audio, de placement et d’adaptateurs mémoire/cache.

Le BIN applicatif mesure 6 102 688 octets (−1 184 par rapport à 0.8.50),
avec 450 912 octets libres dans chaque partition OTA de 6 553 600 octets.
Les sections DATA/BSS totalisent 94 040 octets (+64). IRAM, TCM et la zone
RTC non initialisée sont inchangées. Le cadre GCC de `ie_job_step` passe
de 112 à 128 octets ; il ne représente pas l’ensemble de la pile de tâche.
Les valeurs et leurs preuves sont dans `build-resources-0851.json`.

Un contrôle séparé avec `-Wall -Wextra` et plusieurs erreurs de sûreté
activées a compilé `Tab5ImageCore.c`, `Tab5ImageEditor.cpp` et `Tab5Help.cpp`.
Le cœur Dessin et l’aide ne produisent aucun avertissement. L’interface
produit 51 avertissements, avec les mêmes catégories et textes que la
recompilation des sources 0.8.50 authentifiées : API LVGL dépréciées,
indentation historique, un ancien helper inutilisé et un avis du SDK.
Ils sont conservés intégralement dans les preuves ; ce bilan ne signifie
pas que la compilation globale est exempte d’avertissements.
