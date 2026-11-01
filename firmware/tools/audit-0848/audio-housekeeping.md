# Réveil audio au repos — revue indépendante 0.8.48

## Cause confirmée

`Tab5AudioEngine::service()` de 0.8.47 recréait le worker audio dès que le Wi-Fi
était activé et que cinq secondes s'étaient écoulées, même si aucun état de
puissance, aucune commande et aucun signal à demander n'avaient changé. Le
worker coûtait notamment sa pile réservée de 16 Kio, puis se retirait.

En 0.8.48, l'UI utilise les états Wi-Fi déjà en cache pour détecter soit une
transition de puissance réellement nécessaire, soit la première mesure RSSI de
la connexion, soit une demande explicite de l'écran Wi-Fi. Les requêtes C6 restent
dans le worker. Une nouvelle demande RSSI reçue pendant le RPC reste pendante
par consommation atomique **avant** le RPC. Les bornes de cinq secondes déjà
existantes restent une limitation des tentatives, pas une raison de créer une
tâche. Les commandes de l'utilisateur gardent leur admission prioritaire.

## Propriété des champs

L'UI ne lit les champs ordinaires de l'audio qu'après `active()` faux, qui effectue
une lecture acquire de la publication release finale du worker. Le worker ne
réaccède plus à ces champs après `stopped()`. La demande RSSI, les préférences de
puissance et la demande de restitution sont atomiques. L'UI demeure l'unique
créateur du worker. La revue ne modifie pas le contrôle de priorité ni le cycle
de vie existants.

## Tests réellement exécutés

Le banc exécute les méthodes de production extraites `service`,
`serviceIdleMemoryPriority`, `serviceWifiSignal`, `serviceWifiPower` et
`wifiPowerTarget`, et inclut les véritables classes atomiques de priorité et de
cycle de vie. Les limites RTOS/mutex/RPC/horloge sont contrôlées.

Dans 600 secondes **simulées** de Wi-Fi stable : 120 créations de worker et
120 requêtes RSSI en 0.8.47, une création et une requête en 0.8.48, avec et sans
économie d'énergie. Ce ne sont pas des mesures de CPU ou de latence physique.

Sont également exercés : requête manuelle, première connexion, déconnexion,
Wi-Fi éteint, radio en pause, scan, connexion en cours, priorité mémoire acquise
au repos sans worker, suspension des tâches optionnelles pendant cette priorité,
commande utilisateur pendant la restitution mémoire, échec de création et délai
de nouvelle tentative, worker déjà actif, nouvelle demande pendant le RPC RSSI,
échec de changement de puissance, activité réseau partagée et débordement de
`millis()`. ASan/UBSan passent; LSan n'est pas disponible dans ce sandbox.

Résultats : `tools/verification/audio-housekeeping-0848.json`.

```sh
python3 tools/audio-reset-tests-0848/housekeeping.py --baseline /chemin/0.8.47/Tab5OS_Arduino_v0.8.47_Ecriture --work /tmp/tab5-audio-housekeeping
```
