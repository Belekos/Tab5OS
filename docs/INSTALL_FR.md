# Installer Tab5OS 0.8.53 sur M5Stack Tab5

**Disponibilité :** ce dépôt fournit actuellement les sources. Le firmware précompilé, le paquet SD complet et la fiche Tab5OS dans M5Burner ne sont pas encore publiés. Les noms d'images ci-dessous correspondent aux formats préparés ; ce ne sont pas des liens de téléchargement.

Ce firmware vise le **M5Stack Tab5 / ESP32-P4 avec 32 Mio de PSRAM et 16 Mio de Flash**. Il n'installe ni ne remplace le firmware du coprocesseur ESP32-C6.

## Choisir la méthode

| Situation | Image | Méthode |
| --- | --- | --- |
| Tab5OS déjà installé | `Tab5OS_0.8.53_OTA.bin` | Mise à jour intégrée |
| Première installation | `Tab5OS_0.8.53_initial_install_16MB.bin` | Image fusionnée écrite à `0x0` |

**L'image d'installation initiale remplace toute la Flash du P4, y compris les réglages et données internes. Pour une installation existante, utiliser l'image OTA.** Aucune des deux images ne contient les ressources microSD.

Avant l'installation, sauvegarder ses documents, réglages et clés de chiffrement selon les besoins. Maintenir l'alimentation pendant l'écriture du firmware. Lorsqu'un paquet binaire est disponible, contrôler ses fichiers avec les empreintes SHA-256 fournies.

## Mettre à jour une installation existante

1. Copier l'image OTA correspondante sur la microSD.
2. Dans l'Explorateur, la sélectionner puis choisir **Actions → Installer un firmware**.
3. Utiliser **Vérifier** et comparer le SHA-256 affiché à l'empreinte du paquet.
4. Utiliser **Installer**, attendre la réussite, puis **Redémarrer**.
5. Actualiser les ressources SD selon la procédure ci-dessous.

Cette méthode écrit l'application dans l'autre partition OTA et conserve la table de partitions ainsi que NVS. Ne jamais écrire le BIN OTA à l'adresse `0x0`.

## Première installation avec M5Burner

Ces étapes s'appliquent lorsque la fiche Tab5OS et le paquet SD complet sont publiés.

1. Préparer une microSD FAT32. Copier à sa racine le **contenu** du dossier `sdcard` du paquet complet.
2. Connecter le port USB-C du Tab5 avec un câble de données. Maintenir Reset environ deux secondes, jusqu'au clignotement rapide de la LED verte, puis relâcher pour entrer en mode téléchargement.
3. Dans M5Burner, choisir **Tab5OS 0.8.53**, l'appareil Tab5 et son port série.
4. Écrire l'image fusionnée d'installation initiale à `0x0`. Son bootloader P4 est déjà intégré à `0x2000` ; aucun effacement complet séparé n'est nécessaire.
5. Redémarrer avec la carte préparée, puis régler la langue et le Wi-Fi dans Tab5OS.

Utiliser une version de M5Burner prenant en charge Tab5 / ESP32-P4. L'import M5Burner et le démarrage physique du paquet préparé n'ont pas été testés.

## Préparer ou actualiser la microSD

Utiliser un **paquet de ressources complet correspondant à la version 0.8.53**. Le dossier `firmware/Tab5OS_Arduino_v0.8.53/sdcard` du dépôt est partiel : il ne contient pas les applications compilées ni certaines ressources générées. Le copier seul ne constitue pas une installation complète.

Le paquet complet apporte applications ELF/Python, langues, polices, claviers et ressources hors ligne. Archives Wikipédia, tuiles cartographiques et données de jeux peuvent demander une préparation séparée. Les outils essentiels intégrés restent accessibles sans carte.

Pour une carte neuve, copier le contenu du dossier `sdcard` du paquet complet à sa racine. Pour une carte déjà utilisée :

1. Sauvegarder les fichiers personnels, modules adaptés, réglages, clés, archives Wikipédia, données de jeux et sauvegardes.
2. Copier le dossier `sdcard/Factory/Tab5OS-0.8.53` du paquet vers `/Factory/Tab5OS-0.8.53` sur la carte.
3. Actualiser `MicroPython/lib/tab5factory.py`, `tab5workshop.py`, `tab5fs.py`, `tab5gui.py`, `tab5ui.py`, `tab5apps.py` et `MicroPython/update_sd.py` depuis le paquet, après avoir conservé leurs éventuelles adaptations.
4. Redémarrer, ouvrir `/MicroPython/update_sd.py` dans l'Explorateur et choisir **Réparer et mettre à jour**.
5. Redémarrer à nouveau pour charger les langues, polices et dispositions de clavier actualisées.

**Ne pas remplacer tout le dossier `/System`.** L'action distincte **Restaurer les fichiers modifiés** remplace volontairement les ressources personnalisées. Le réglage de restauration des fichiers manquants ne recrée que les petites ressources absentes ; il n'actualise pas les fichiers existants et ne fournit pas toutes les données externes.

## Installer depuis les sources

Le [guide de compilation](BUILD.md) décrit les dépendances, les scripts et les fichiers produits. Compiler le firmware ne crée pas automatiquement un paquet SD complet ni une image d'installation initiale.

Références officielles : [M5Burner](https://docs.m5stack.com/en/uiflow/m5burner/intro) · [Mode de téléchargement Tab5](https://docs.m5stack.com/en/uiflow2/Tab5/program).
