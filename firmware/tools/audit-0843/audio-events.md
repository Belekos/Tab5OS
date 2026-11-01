# Événements de l’indicateur audio — 0.8.43

## Diagnostic sur la référence 0.8.42

`updateTopAudioIndicator()` copiait un `Tab5AudioSnapshot` complet (six `String`) toutes les 250 ms, y compris à l’accueil lorsque le lecteur était arrêté et l’indicateur caché. `updateClockLabels()` copiait également ce snapshot pour lire seulement deux booléens Wi-Fi. Le correctif .42 des accesseurs scalaires n’avait pas supprimé ces copies faites explicitement par la boucle principale.

Le producteur audio publie déjà un état partagé protégé ; ses tâches, pilotes et commandes restent inchangés. `takeUiDirty()` est un événement consommable utilisé par d’autres vues : l’employer pour le bandeau ferait perdre un événement à un autre consommateur. Aucun lien entre ces copies et un flash DSI physique n’est démontré.

## Correction

- `indicatorSnapshot(out, seen, force)` utilise une révision atomique indépendante, non consommable. Tant qu’elle est inchangée, aucun verrou et aucune copie de texte. Chaque observateur garde son propre `seen`.
- La révision change uniquement sur mode, pause, volume, titre, chemin local ou URL de la station. Ces champs couvrent exactement le bandeau et `audioRememberPlayback()`. La progression, le niveau d’enregistrement, les buffers et le RSSI restent dans les snapshots des vues qui les affichent.
- Si un changement existe, acquisition immédiate du mutex (`timeout=0`) puis copie cohérente. Une occupation du mutex laisse l’événement en attente. Un échec silencieux de copie `String` des trois champs d’identité ne valide pas la révision : l’interface peut réessayer sans conserver définitivement un titre vide.
- `wifiSnapshot(enabled, connected)` lit les deux booléens sous un seul verrou immédiat, sans `String` ni requête réseau. En cas d’occupation, les sorties du dernier appel réussi sont conservées par le contrôleur.
- Le snapshot complet et `takeUiDirty()` restent disponibles avec leurs contrats précédents. Aucun minuteur, worker, callback réseau, queue ou ressource de document supplémentaire. Le compteur atomique coûte quatre octets par moteur audio.

L’intégration principale emploie la révision au lieu du minuteur de 250 ms ; les changements de page demandent une publication forcée. Une demande forcée doit rester en attente jusqu’à une copie réussie. Les profils de lecture et le dernier fichier/station gardent leur comportement.

## Mesures et tests exécutés

Vraies méthodes `publishSnapshot`, `snapshot`, `takeUiDirty`, `indicatorSnapshot` et `wifiSnapshot`, ainsi que la vraie déclaration du snapshot, compilées sur hôte avec adaptation de `String`, mutex et périphériques. ASan/UBSan activés. Le compteur de copies ne mesure pas les allocations réelles de l’ESP32 ni une latence physique.

| Modèle de 60 secondes, producteur identique à 50 ms | Référence .42, lecteur à 250 ms | .43, lecteur à 8 ms |
|---|---:|---:|
| Copies de `String` après la lecture initiale | 1 440 | 0 |
| Prises de mutex demandées par le lecteur | 240 | 0 |
| Prises de mutex du producteur | 1 199 | 1 199 |

La nouvelle lecture peut donc être consultée à chaque tour UI sans revenir à un polling de snapshots coûteux. La publication périodique du producteur audio n’est pas supprimée : elle dessert aussi la progression et les autres vues.

Tests : observateurs indépendants ; force et retry ; interopérabilité avec l’ancien événement consommable ; chaque champ pertinent ; absence de notification pour progression/statut ; mutex occupé ou non initialisé ; copie silencieusement refusée pour chacun des trois textes pertinents ; bouclage du compteur sans publier la valeur initiale zéro ; état Wi-Fi cohérent sans copie. Aucun essai sur Tab5 physique.

Preuves : `audio-events-before.json`, `audio-events-after.json` ; script reproductible `tools/audio-events-tests-0843/run.py`. L’objet `Tab5Audio.cpp` est compilé avec succès pour le P4, avec le SDK M5Stack3.3.9/GCC RISC-V14.2 restauré : `tools/verification/audio-events-target-0843.json`. La validation complète du firmware appartient au lot compilation.
