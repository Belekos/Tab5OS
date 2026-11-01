# Tâche audio au repos — 0.8.31

Exécuter depuis la racine de la livraison :

```sh
python3 tools/audio-worker-tests-0831/run.py
```

Nécessite Python3 et g++ avec ASan/UBSan et pthread. Aucun téléchargement.
L’option `--build /chemin/temporaire` conserve le programme et son journal.

Le script extrait les méthodes de production `audioTaskEntry`,
`canRetireAudioTask`, `reclaimIdleWorker`, `startAudioTask`, `service` et
`acquireMemoryPriority`. Il utilise également les en-têtes exacts de cycle de
vie, trampoline de tâches et priorité audio. La création des tâches est
simulée par des threads hôtes ; le tas, les pilotes et le réseau sont simulés.

Les scénarios vérifient l’absence de pile audio permanente sans travail,
les refus d’admission et de création avec conservation de la commande,
l’absence de décodage/RPC sur le thread UI, la publication du démarrage avant
l’accès au moteur, la protection de la lecture/enregistrement/connexion/scan,
la libération à l’arrêt, le report des tâches Wi-Fi facultatives sous pression,
le prêt prioritaire demandé sans tâche audio et 400 cycles de commandes
concurrentes à la retraite d’une tâche.

Le compteur d’assertions et le nombre exact de tâches créées varient avec
l’ordonnancement hôte. Les 400 cycles, l’absence de commande perdue et le retour
à zéro des piles simulées et réservations sont obligatoires.

Ce test ne mesure pas la RAM d’une Tab5, ne lance pas FreeRTOS, ne contacte pas
le C6 et ne décode aucun flux. La pile demandée par le firmware reste 16 Kio.
Sa restitution matérielle dépend du nettoyage normal des tâches FreeRTOS.
Le rapport `tools/verification/audio-worker-0831.json` lie les sources et
fixtures testées par SHA-256. LeakSanitizer est désactivé à cause de
l’inspection de processus indisponible dans cet environnement ; ASan et
UBSan restent actifs.
