# Vérification du gestionnaire mémoire 0.8.31

Exécution : `python3 tools/memory-tests-0831/run.py` (GCC C++17 et pthreads).

Le programme compile **Tab5ResourceManager.cpp réel**, sous AddressSanitizer et UndefinedBehaviorSanitizer. Seules les mesures de tas et les ressources libérables sont simulées. Il vérifie notamment le cas RAM interne 46 Kio / 48 Kio nécessaires, le choix du bon type de mémoire, la fragmentation, les réservations simultanées, l’attente bornée d’un retrait différé de pile, les demandes depuis un worker et leur expiration.

Les politiques d’inactivité sont testées avec le retour au premier plan, le refus d’un callback dont les données sont utilisées, le passage du compteur de temps à zéro et la limite d’un callback d’expiration par seconde. Les 400 cycles de réserve/allocation/libération vérifient l’absence de réserve résiduelle et de destruction inutile des caches PSRAM. Les marges de sécurité restent de 32 Kio internes et 512 Kio de PSRAM.

Les callbacks d’application déclarent les domaines qu’ils peuvent libérer et restent responsables de refuser lorsque leurs données sont utilisées. Une notification de retrait de tâche ne compte pas comme de la mémoire déjà rendue : seule une mesure du tas après libération permet une nouvelle admission.

`tools/verification/memory-0831.json` lie le résultat aux empreintes des sources et tests. L’empreinte séparée du bloc d’enregistrement des callbacks identifie l’intégration examinée ; elle ne prétend pas exécuter l’interface Arduino sur l’hôte.

Limites : pas de matériel Tab5, de FreeRTOS réel, ni de Wi-Fi ou clone réseau dans cette suite. Le cycle de vie audio est vérifié par sa suite dédiée. LeakSanitizer est désactivé dans cet environnement hôte ; AddressSanitizer et UndefinedBehaviorSanitizer restent actifs.
