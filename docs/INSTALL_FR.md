# Installer Tab5OS 0.8.53 sur M5Stack Tab5

**Disponibilité :** ce dépôt de sources ne fournit ni firmware à télécharger ni paquet SD complet. Les procédures ci-dessous décrivent les formats vérifiés de la 0.8.53, à utiliser lorsqu’une livraison redistribuable sera publiée. Aucune fiche M5Burner n’est annoncée en ligne. Voir [NOTICE.md](../NOTICE.md).

Cette livraison vise le **Tab5 avec ESP32-P4 et 32 Mio de PSRAM**. Elle utilise la cible de compilation M5Stack `prev3`, une Flash de 16 Mio et le mode DIO. Ce n’est pas un firmware ESP32-S3, ESP32-C6 ou un système Linux. Le firmware du coprocesseur C6 n’est pas inclus ni modifié par ces fichiers. Les autres révisions matérielles restent à vérifier ; ce constat ne constitue pas un verrou logiciel supplémentaire.

## Choisir le fichier adapté

| Situation | Fichier | Méthode |
| --- | --- | --- |
| Tab5OS déjà installé, données et paramètres à conserver | `firmware/Tab5OS_0.8.53_OTA.bin` | Mise à jour depuis Tab5OS |
| Première installation sur un appareil sans données à conserver | `firmware/Tab5OS_0.8.53_initial_install_16MB.bin` | Image fusionnée, écrite à `0x0` |

**L’image d’installation initiale remplace les 16 Mio de Flash du P4, y compris NVS, paramètres et données internes. Ne pas l’utiliser pour mettre à jour une installation personnelle.** La microSD n’est pas incluse dans cette image. Sauvegarder ses fichiers avant toute préparation ; ne pas formater une carte contenant des données à conserver.

Dans un paquet de firmware, les empreintes figurent dans `SHA256SUMS.txt` et `python3 verify_package.py` contrôle les images sans accéder à un appareil. Ces fichiers du paquet binaire ne sont pas inclus dans ce dépôt de sources.

## Mise à jour depuis Tab5OS

1. Copier `Tab5OS_0.8.53_OTA.bin` sur la microSD, dans un emplacement libre choisi.
2. Dans l’Explorateur, sélectionner ce fichier puis **Actions → Installer un firmware**.
3. Utiliser **Vérifier** et comparer l’empreinte affichée à `SHA256SUMS.txt`. Le descriptif technique embarqué du SDK n’est pas un nom de version fiable ; l’empreinte identifie précisément cette livraison.
4. Utiliser **Installer**. Attendre la réussite, puis **Redémarrer**. Ne pas interrompre l’alimentation pendant l’écriture.
5. Actualiser les ressources SD avec la procédure ci-dessous.

Ce parcours écrit l’application dans l’autre partition OTA. Il ne remplace ni la table de partitions ni NVS. Le BIN OTA seul ne doit jamais être écrit à `0x0`.

## Première installation avec M5Burner

Utiliser une version de M5Burner prenant en charge **Tab5 / ESP32-P4**, et l’entrée Tab5OS publiée par le propriétaire du projet. Les pages de publication et la reconnaissance effective du paquet par M5Burner nécessitent encore une validation dans le service ; aucun flash matériel n’a été exécuté lors de sa préparation.

1. Préparer la microSD et vérifier les sauvegardes nécessaires. Pour une carte neuve en FAT32, copier à sa racine le **contenu** du dossier `sdcard` de la livraison correspondante. Ne pas copier seulement l’archive ZIP.
2. Connecter le Tab5 par son port USB-C avec un câble de données. Maintenir Reset environ deux secondes, jusqu’au clignotement rapide de la LED verte, puis relâcher : c’est le mode de téléchargement décrit par M5Stack.
3. Dans M5Burner, choisir l’entrée **Tab5OS 0.8.53**, l’appareil Tab5 et son port série. Le paquet d’installation correspond à une image fusionnée écrite à `0x0` ; le bootloader P4 est déjà intégré à `0x2000`.
4. Lancer l’installation initiale uniquement après avoir accepté le remplacement des données internes décrit plus haut. Ne pas ajouter une opération séparée d’effacement complet.
5. Après la réussite du transfert, redémarrer avec la microSD préparée. Régler la langue et le Wi-Fi dans Tab5OS. Aucun identifiant Wi-Fi personnel n’est contenu dans l’image distribuée.

Si l’entrée M5Burner n’est pas encore publique, ce dossier est une préparation de publication, pas une preuve de mise en ligne. Le champ `Github` doit pointer vers le dépôt réellement créé avant publication.

## Ressources microSD

Une nouvelle installation utilise les ressources de la **même version 0.8.53** : langues, polices, claviers, modules Python, applications externes et index local des villes. Elles sont livrées séparément du firmware. Les applications essentielles intégrées restent accessibles sans SD, mais les fonctions dépendant de fichiers externes sont alors limitées.

Pour une carte déjà utilisée, **ne pas écraser l’ensemble de `/System`**. Sauvegarder les personnalisations, copier `/Factory/Tab5OS-0.8.53`, actualiser les modules de réparation listés dans [`LISEZ_MOI.md`](../firmware/LISEZ_MOI.md), puis ouvrir `/MicroPython/update_sd.py` et utiliser **Réparer et mettre à jour**. L’option **Restaurer les fichiers modifiés** remplace volontairement des personnalisations et ne constitue pas la mise à jour normale. La procédure complète du dépôt fait foi.

## Périmètre de validation

Les deux fichiers sont issus de la compilation ESP32-P4 vérifiée de la 0.8.53. L’intégrité de l’application, le contenu de l’image fusionnée, les offsets, la table de partitions, son MD5 et le CRC de sélection OTA sont contrôlés. Les zones NVS, app1, SPIFFS et coredump de cette image d’usine sont remplies de `0xFF` ; aucun export de tablette personnelle n’a servi à sa construction.

Aucun essai d’import M5Burner ni de démarrage physique n’est revendiqué. Voir les vérifications du projet et [`TESTS_TAB5_0.8.53.md`](../firmware/TESTS_TAB5_0.8.53.md) avant d’annoncer une validation matérielle.

Références officielles : [M5Burner](https://docs.m5stack.com/en/uiflow/m5burner/intro), [mode de téléchargement Tab5](https://docs.m5stack.com/en/uiflow2/Tab5/program), [publication](https://docs.m5stack.com/en/uiflow/m5burner/publish).


Table de partitions : la table effective est `tools/partitions/default_16MB.csv` dans le SDK ; le sketch ne contient pas `partitions.csv`. Voir les [précisions de compilation](BUILD.md).
