# Registre d’ingénierie 0.8.49

## Référence et critère de décision

Source : archive 0.8.48 réellement livrée, SHA-256 `599483c491c253a4e41c813b2e94aecf30e4d3c85d3459486dc637e1abac095b`. Son échec aux essais utilisateur est la référence matérielle ; ses tests logiciels ne démontrent pas la disparition des flashs ou des claquements.

Référence historique audio : 0.4.10, archive SHA-256 `e075642af455705c6cb8fab8c5a4981bc98d92f9635086c09bf74f425e4a7f0e`. Elle utilisait déjà des buffers graphiques PSRAM par défaut. Le diagnostic LVGL déclaré sans flash par l’utilisateur n’a pas été retrouvé sous forme de sources vérifiables : aucune comparaison binaire prétendue.

## Priorités et preuves

| Priorité | Mécanisme établi | Effet possible / preuve restante |
|---|---|---|
| P1 | Parcours de tous les blocs sous verrou critique du SDK, répétés par admissions et diagnostics automatiques | Retard du réarmement DSI ; corrélation temporelle physique à mesurer |
| P1 | Réallocation LVGL dans le heap partagé ; déplacement/copie sous verrou IRQ du SDK | Longue interruption du service DSI lors de reconstruction d’objets |
| P1 | Synchronisation de grandes plages de cache sous verrou critique partagé | JPEG/ELF et autres producteurs peuvent bloquer les deux cœurs |
| P1 | Abandon silencieux de release audio après timeout mutex | État arrêté mais propriété/PCM conservés ; code confirmé |
| P1 | Stop logique confondu avec destruction I²S/codec/ampli | Multiplication des transitoires au démarrage ; mesure acoustique nécessaire |
| P2 | Vidéo terminée conservant trois frames lorsque seule l’image affichée reste nécessaire | Jusqu’à deux frames restituables sans suppression de l’image finale |
| P2 | Admission radio imposant 1 Mio contigu avant ring adaptatif pouvant descendre à 64 Kio | Échec prématuré malgré une configuration moins volumineuse possible |
| P2 | Pages masquées conservant des catalogues jusqu’à pression mémoire | Politique de cache implicite ; ne pas confondre avec document, pause ou fuite |

## Invariants exigés

- Aucun tampon partagé n’est libéré avant acquittement de son consommateur.
- L’admission reste indicative : seule l’allocation finale possède les octets. Aucun chiffre de bloc disponible inventé.
- Les erreurs d’allocation conservent l’ancien objet et la possibilité de fermer proprement.
- Les appels LVGL restent dans leur propriétaire UI ; les mutex ordinaires ne sont pas remplacés par des spinlocks longs.
- Le découpage de cache conserve validation, ordre, alignement et visibilité avant DMA.
- Pas de nouveau formatage, migration de partitions, effacement de préférences, autosauvegarde forcée ou retrait de fonction.
- Les tests compilent les sources 0.8.49, avec leurs empreintes ; les rapports hérités restent historiques.

## Portée des mesures

Les comptes de parcours de blocs sur hôte caractérisent la complexité ; ils ne sont pas des latences ESP32-P4. La taille du binaire et les sections ELF seront mesurées par compilation cible. RAM libre au repos, underruns DSI, latence tactile, bruit analogique et fuite sur appareil nécessitent la Tab5 physique. Aucun test de simulation ne doit être présenté comme une validation de ces symptômes.
