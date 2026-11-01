# Partage de fichiers

Aucune installation sur l’ordinateur. Le partage donne accès aux fichiers dans
un navigateur moderne, en lecture et en écriture. Ce n’est pas un disque USB
ni un lecteur réseau avec une lettre Windows.

1. Ouvrir **Réglages → Système → Partage**.
2. Choisir **Réseau actuel** si le PC et la tablette sont sur le même réseau.
   Ou désactiver le Wi-Fi dans les réglages, puis choisir **Réseau direct** :
   la tablette crée un réseau temporaire protégé par un mot de passe. Connecter
   le PC à ce réseau avec le nom et le mot de passe affichés sur la tablette.
3. Ouvrir dans le navigateur l’adresse affichée, par exemple
   `http://192.168.4.1:8080`, puis saisir le code à six chiffres.
4. Naviguer dans les dossiers, télécharger des fichiers, en envoyer plusieurs,
   créer un dossier, renommer ou supprimer un élément.
5. Arrêter le partage sur la tablette une fois terminé. Il s’arrête aussi
   automatiquement après 30 minutes. Le réseau direct disparaît alors. La commande **Arrêter** est visible uniquement pendant un partage actif.

Le réseau direct fonctionne sans routeur et sans Internet. Il ne modifie pas
le réseau mémorisé ni l’option de connexion automatique. Les réglages Wi-Fi
restent bloqués pendant un partage pour éviter une coupure en pleine copie.

Un remplacement demande confirmation. Les fichiers remplacés ou supprimés
sont conservés dans `/.Tab5Trash/<identifiant>/<nom original>`. Ce dossier est
visible dans le navigateur : on peut télécharger une ancienne version, puis
supprimer définitivement les fichiers devenus inutiles. Il n’est jamais vidé
automatiquement. Les dossiers doivent être vides avant suppression.

Les transferts utilisent un tampon de 16 Kio, sans charger le fichier entier
sur la tablette. La taille maximale d’un fichier est de 2 147 483 647 octets
(moins de 2 Gio), limite actuelle des positions de fichiers du SDK. Les volumes
de cartes de 1 900 Mio restent compatibles. Les envois incomplets sont nettoyés
lors d’une interruption ordinaire ; après une coupure d’alimentation, un fichier
`.t5upload-…` peut rester dans le dossier cible. Ce fichier temporaire peut être
supprimé. Une panne pendant un remplacement peut laisser l’ancienne version
uniquement dans `/.Tab5Trash` ; elle y reste récupérable.

Les modifications finales attendent une période sans travail SD incompatible.
Si la carte reste occupée, fermer le document en cours ou arrêter l’enregistrement,
puis réessayer. Une radio réseau peut rester active. Une seule requête de fichiers
est traitée à la fois ; éviter de lancer un téléchargement et un envoi simultanés.
Le partage HTTP est limité au sous-réseau local choisi. Il est activé uniquement
à la demande et nécessite le code affiché sur la tablette.
