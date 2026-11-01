# Bibliothèques et projets MicroPython — 0.8.19

Depuis **Code → Bibliothèques**, **Atelier → Bibliothèques** ou le script MicroPython `libraries.py` :

- **Catalogue sur SD** : retrouver les bibliothèques par famille de carte et par usage.
- **Importer SD / USB** : copier un module `.py`, un module `.mpy` ou un dossier décompressé déjà présent sur la carte SD / clé USB.
- **Obtenir les sources par Wi-Fi** : télécharger un paquet mip, une adresse HTTPS ou une source `github:`, avec ses dépendances mip. Les sources restent ensuite utilisables hors ligne.
- **Nouveau projet MicroPython** : créer un dossier avec `main.py` et une fiche de cible.
- **Modules installés pour la Tab5** : retrouver la gestion existante des modules utilisés par les scripts exécutés sur cette Tab5.

## Rangement et utilisation

Le catalogue utilise `/MicroPython/Bibliotheques/<carte>/<usage>/<nom>/lib/`. Les informations de classement sont placées dans `metadata.json` à côté des fichiers. Les métadonnées de paquets amont et les sources ne sont pas exécutées pour établir la compatibilité. Le catalogue n’est jamais ajouté au chemin global de recherche Python.

Les cibles proposées sont : portable / à vérifier, Tab5 P4, ESP32, S3, C3, C6, P4 autre carte, ESP8266, RP2040, RP2350, STM32 et autre carte. Les usages sont : général, capteurs, écran, réseau, stockage, moteurs et audio. Cette indication est une **déclaration choisie par l’utilisateur**, pas un résultat de test matériel.

Un import de dossier conserve son arborescence. Si le dossier contient `__init__.py` ou `__init__.mpy`, son nom est conservé comme nom du paquet Python. Sinon, ses contenus sont placés directement dans `lib/`. Choisir donc le dossier du paquet ou le contenu du dossier `lib`, pas la racine complète d’un dépôt. Les fichiers annexes ordinaires sont conservés ; les dossiers cachés, `__pycache__` et `node_modules` sont ignorés. Les ZIP doivent être décompressés avant import. Les dépendances d’un import local doivent être présentes dans le dossier ou ajoutées séparément.

Dans une bibliothèque, **Ajouter à un projet** permet de créer un projet ou de choisir un dossier existant sur la SD. Un dossier USB est refusé avant toute écriture : le journal transactionnel réside sur la SD. L’USB reste utilisable comme source d’import. Les sources et leurs arborescences sont copiées dans son sous-dossier `lib/`. Une copie identique est réutilisée ; un fichier différent bloque l’ensemble de l’ajout avant remplacement. Les sources personnelles du projet sont conservées. Les fichiers modifiés dans le catalogue depuis leur import bloquent aussi l’ajout : les réimporter sous un autre nom pour conserver les deux variantes.

Les nouveaux projets se trouvent dans `/MicroPython/Projets/<nom>/main.py`. Ouvrir ce fichier dans **Écriture** ; pour l’inscrire dans les projets de l’éditeur, utiliser **Documents → Importer un dossier**. La fiche `.tab5-project.json` stocke uniquement la cible déclarée et les bibliothèques ajoutées. Elle ne modifie pas les réglages de la carte externe.

## Portabilité et limites

Une source `.py` peut dépendre d’un module, d’un périphérique ou d’un brochage absent de la cible. La cible déclarée ne valide aucun de ces points. Un `.mpy` est conservé sans exécution : seul son en-tête minimal est contrôlé à l’import ; la version MicroPython, les paramètres du firmware et, pour le code natif, l’architecture restent à vérifier sur la carte destinataire. Le gestionnaire ne convertit pas les `.mpy`. Les téléchargements mip utilisent les sources `.py`.

Ce fonctionnement prépare de vrais fichiers et projets sur SD **sans ordinateur**. Le transfert de ces fichiers vers une carte, le flashage de son firmware et le lancement du programme ne sont pas ajoutés par cette évolution. Exécuter un script dans Tab5OS le lance sur la Tab5, quelle que soit la cible déclarée de son dossier. La VM peut charger les modules du sous-dossier `lib` du script ; les modules système Tab5 restent prioritaires.

Pour préserver la mémoire et la fluidité, un import local est limité à 160 fichiers, 1 Mio par fichier, 8 Mio au total et 8 niveaux. Le parcours utilise un seul des huit descripteurs de dossier du pont natif ; il rend régulièrement la main. Les copies utilisent des blocs de 4 Kio. Le navigateur charge les fiches de classement sans charger les sources ni conserver leurs tables d’empreintes. Au-delà de 256 bibliothèques affichées, filtrer par cible ou par usage. Le téléchargement mip conserve ses limites de 32 paquets, 128 sources et 4 Mio.

Les opérations utilisent le journal de récupération existant : une interruption de copie ou de commit doit restaurer les fichiers précédents. Les temporaires internes de téléchargement sont désormais reconnus par la récupération ; une coupure pendant un téléchargement ne doit plus bloquer toutes les opérations de l’Atelier.

## Vérifications et limites

Les suites automatisées conservées utilisent un système SD simulé et couvrent : import sans exécution, paquet avec `__init__`, dépendances mip isolées, projets, copie répétée, conflits et fichiers personnels, source de catalogue modifiée, `.mpy`, protection des modules système, échec réseau et interruption du commit. Seuls les passages explicitement consignés dans VERIFICATION.md constituent des validations de la version courante. L’ergonomie tactile, les performances et la reprise après retrait physique de la SD restent à vérifier sur Tab5.
