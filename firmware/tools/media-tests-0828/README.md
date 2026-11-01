# Média : persistance, catalogue et interface 0.8.25

Le volume est conservé en NVS (`tab5_media/volume`), indépendamment de la microSD. À la première utilisation, la valeur existante des réglages SD est reprise, y compris zéro. Si les anciens réglages sont temporairement indisponibles (microSD ou migration), le volume de démarrage n'est pas enregistré à leur place ; une récupération ultérieure reprend le volume ancien seulement si aucun choix utilisateur ni volume NVS n'existe. Les réglages de volume du lecteur et des applications ELF passent par `Tab5Audio.setVolume()`. Le contrôle SDR doit utiliser le même point d'entrée. Le volume temporaire d'une alarme reste distinct.

`Tab5MediaSession` ne lance jamais de lecture. Il conserve le dernier onglet Média, les deux filtres de favoris, les dossiers audio/radio, la sélection et le titre pour chaque type, les recherches et le défilement. La sélection est une identité stable (chemin ou URL), pas un index du catalogue. Le bouton Lecture résout cette identité dans le catalogue courant, en respectant le dossier, les favoris et la recherche affichés. Un fichier disparu n'est pas ouvert sous l'index d'un autre fichier.

Les modifications sont regroupées après 1,2 seconde de repos. La sortie de Média, le relâchement du curseur de volume et la préparation de la veille demandent aussi une sauvegarde. Les mises à jour inchangées ne provoquent aucune écriture. Un échec reste en attente et est réessayé au plus toutes les cinq secondes. Le navigateur utilise un enregistrement versionné avec contrôle de cohérence ; un enregistrement invalide rétablit le navigateur initial sans perdre le volume indépendant. Une coupure d'alimentation pendant le délai de regroupement peut perdre le dernier changement non encore enregistré.

Le blob de navigation et sa copie transitoire sont alloués sur le tas selon la politique PSRAM de l'OS, sans grand tampon sur la pile UI. Les identités sont bornées à 511 octets et ne sont jamais enregistrées tronquées. Les positions, titres et recherches n'entraînent aucune écriture SD pendant le rendu ou le défilement.

`Tab5MediaList` conserve seulement des références vers le catalogue. Son nombre d'objets graphiques reste fixé à 24 lignes, même pour 800 stations. La navigation clavier suit l'index logique ; un clic commencé avant le recyclage d'une ligne ou le changement de dossier ne peut pas activer un autre média. Les favoris sont des étoiles vectorielles, pleines ou vides, indépendantes des polices de la langue. La suppression des enregistrements conserve sa corbeille.

Le tri partage les six ordres de l'explorateur pour les fichiers : nom, date et taille, dans les deux sens. Les radios, qui n'ont ni taille ni date de fichier pertinente, proposent les deux ordres de nom. Les métadonnées de fichiers sont lues une fois avant le tri par date/taille ; le comparateur et le rendu ne relisent pas le stockage. Les données absentes (par exemple une vidéo distante) valent zéro pour ces tris. Les clés temporaires de tri sont libérées immédiatement.

Le catalogue natif accepte jusqu'à 800 stations dans un fichier de 256 Kio au maximum. Son parseur utilise l'allocateur JSON PSRAM et ne conserve que les champs nécessaires. Le paquet public de 600 stations est compatible avec cette limite, sans créer de favoris ni lancer de radio à l'installation.

## Vérification

Lancer `python tools/media-tests/run.py` depuis la distribution (le script localise ses sources). Le banc utilise les dépendances hôtes disponibles dans l'environnement de développement et produit `tools/verification/media-0825.json`.

- Le module NVS de production est exécuté avec un stockage simulé : redémarrage, zéro, échec et reprise d'écriture, contrôle d'intégrité, chaînes bornées, débordement de l'horloge et 4 000 mises à jour inchangées.
- La liste de production est exécutée avec LVGL et le gestionnaire de focus réels : 800 entrées, 24 lignes, clavier, souris, favoris, tris, clics périmés et destruction.
- Les fonctions actuelles du chargeur radio sont exécutées avec ArduinoJson, un stockage simulé et le vrai paquet de 600 stations : format objet/tableau, stations désactivées, 800 maximum, JSON invalide et dépassement de taille.

- La page et les callbacks Média actuels du `.ino` sont extraits sans modification et exécutés avec LVGL : mise en page à 24 pixels, favoris, retour d'onglet, sélection puis Lecture, 600 stations, corbeille, récupération après libération du cache, volume et suivi en arrière-plan. Le rapport contient les empreintes de ces fonctions et des variables globales utilisées.

Ces contrôles ne sont pas des mesures sur tablette. Le pilote audio, le comportement matériel NVS et la fluidité réelle sur SD restent à vérifier avec la compilation cible et des essais sur l'appareil.
