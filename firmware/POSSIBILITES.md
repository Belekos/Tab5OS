# Extensions possibles pour Tab5OS

État actualisé pour la 0.8.19, le 10 octobre 2026. Priorité : fiabilité des outils et fonctionnement autonome. Les développements plus larges ci-dessous restent des possibilités.

Le terminal enrichi et l’application Pentest font partie de la 0.8.19. Le VPN et Tor restent à intégrer ; leur évaluation et les limites de supervision réseau figurent dans [RESEAU_DIAGNOSTIC.md](RESEAU_DIAGNOSTIC.md).

## Projets MicroPython pour d’autres cartes

Le catalogue classe les sources par cible déclarée et par usage, les importe depuis SD/USB ou mip et les copie dans les projets sur SD. On peut préparer et éditer ces projets sans ordinateur. Les bibliothèques restent séparées des modules système du Tab5. Voir MICROPYTHON_BIBLIOTHEQUES.md.

Le transfert vers une carte externe, son flashage et la vérification de son brochage restent des travaux distincts. Aucun téléverseur universel ni compatibilité automatique ne sont promis.

## Jeux et émulateurs

GB, GBC, NES, Master System/Game Gear et Maze3D étaient déjà fournis. GBA, SNES, MegaDrive et Doom restent proposés ; Quake et Duke Nukem 3D sont retirés en 0.8.16. BINAIRES.md décrit les formats, limites de mémoire, commandes et données nécessaires.

Les formats PAPP amont ne sont pas chargés directement : les moteurs sont portés vers l’ABI Tab5OS. Ils utilisent l’affichage, le son, le stockage et les entrées de l’OS. La fluidité, les jeux à puces particulières, les mods et les périphériques supplémentaires demandent encore une validation sur Tab5.

## RTL-SDR

Un affichage FFT avec spectre et historique, puis des démodulateurs ciblés, est techniquement plausible en C/C++. La 0.8.13 fournit le contrôle du RTL2832/tuner et des transferts USB bornés à 240 kI/Q/s, avec spectre et AM/FM mono. Les hauts débits, l’historique spectral et les décodeurs supplémentaires restent à développer. Les décodeurs doivent être ajoutés et mesurés un par un ; il n’existe pas de promesse réaliste de décodage universel.

À 2,4 Méch/s, deux octets I/Q par échantillon représentent environ **4,8 Mo/s**, avant surcoûts. Le mode USB par défaut force FS/LS pour permettre les hubs de claviers/souris ; le nouveau choix USB rapide permet HS après redémarrage. La pile IDF 5.5 ne sait pas traduire les transactions de périphériques FS/LS derrière un hub HS. Il faut donc valider une configuration HS pour le SDR et traiter séparément la coexistence avec les périphériques d’entrée. Ne pas simplement retirer le contournement actuel des hubs.

La largeur observée simultanément est celle de l’échantillonnage ; examiner une bande plus large exige de balayer les fréquences. Un RTL-SDR 8 bits n’est pas un analyseur de laboratoire étalonné. Une première preuve de fonctionnement via `rtl_tcp` et un récepteur distant peut valider l’interface/DSP, mais ne répond pas à elle seule à la demande de branchement USB direct.

Sources : [API rtl-sdr](https://github.com/osmocom/rtl-sdr/blob/master/include/rtl-sdr.h), [implémentation USB](https://github.com/osmocom/rtl-sdr/blob/master/src/librtlsdr.c), [rtl_tcp](https://github.com/osmocom/rtl-sdr/blob/master/src/rtl_tcp.c), [limites USB IDF 5.5 P4](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/peripherals/usb_host.html).

## Meshtastic permanent

Un client natif est réalisable via un nœud Meshtastic externe et son protocole Protobuf : liaison série/UART, USB série avec son pilote, ou TCP sur le réseau local. La Tab5 et son C6 n’ont pas de radio LoRa intégrée. La voie BLE demanderait également de vérifier le firmware esp-hosted du C6 et son intégration au système.

Écran éteint, l’OS peut continuer à écouter avec une petite tâche et une file bornée. En veille profonde ou appareil éteint, le nœud externe doit assurer lui-même la réception. L’historique durable et un éventuel réveil par signal matériel doivent être conçus explicitement ; la mémoire temporaire d’un nœud ne garantit pas la conservation illimitée des messages pendant l’absence de la tablette. Pour la basse consommation, un nœud LoRa autonome avec sa propre alimentation est une architecture plus adaptée que le maintien permanent du P4 et du Wi-Fi.

La confidentialité dépend du canal et des clés. Le canal par défaut est public en pratique, avec sa clé connue ; il faut des paramètres privés et une vérification des correspondants pour les usages privés.

Sources : [API Meshtastic](https://python.meshtastic.org/), [protocole hôte](https://github.com/meshtastic/meshtastic-sdk/blob/main/docs/protocol.md), [énergie](https://meshtastic.org/docs/configuration/radio/power/), [messages et canaux](https://github.com/meshtastic/meshtastic/blob/master/docs/software/android/user/messages-and-channels.md).

## Communication sécurisée

Une messagerie texte avec pièces jointes bornées est envisageable. Il faut choisir un protocole éprouvé, gérer l’identité et la vérification des contacts, protéger les clés et l’historique, traiter rejouements/pertes/reprises et prévoir le relais éventuel. TLS protège la liaison au serveur ; il ne suffit pas à promettre un chiffrement de bout en bout. La sécurité d’une communication doit être évaluée sur l’ensemble de ces composants. Aucune application Signal complète n’est ajoutée ici.

Les spécifications [Double Ratchet](https://signal.org/docs/specifications/doubleratchet/) et [gestion des sessions Sesame](https://signal.org/docs/specifications/sesame/) illustrent notamment pourquoi cela dépasse une simple fonction AES ou l’échange d’un SHA.

## Courriel — hors distribution principale

Mail est retiré en 0.8.16. Les considérations suivantes ne constituent pas une fonction disponible.

Lecture et envoi sont réalistes avec IMAP/SMTP sur TLS : consultation des en-têtes d’abord, chargement progressif du corps, pièces jointes vers la SD, synchronisation bornée et identifiants conservés sous la protection du coffre. ReadyMail est une base Arduino/C++ à évaluer, avec une attention particulière à ses limites et à la validation des certificats. Ses exemples utilisant `setInsecure()` ne doivent pas être repris pour une application sécurisée.

L’authentification dépend du fournisseur : mot de passe d’application ou OAuth selon le compte. Une éventuelle réintégration devrait valider la connexion, la lecture, l’envoi et la fermeture sur appareil pour les fournisseurs visés. Il s’agit d’un client mail. L’envoi par TLS n’est pas, à lui seul, un chiffrement de bout en bout du message.

Source : [ReadyMail](https://github.com/mobizt/ReadyMail).

## Navigateur Internet — hors distribution principale

Le navigateur intégré en 0.8.14 est retiré en 0.8.16. Les services réseau des autres outils restent disponibles.

Un navigateur équivalent à Chromium/Firefox, exécutant les sites modernes et leurs applications JavaScript, n’est pas un objectif réaliste dans les 32 Mio partagés de cette Tab5. Un rendu distant est une autre architecture, avec une dépendance au réseau et au serveur. Pour Wikipédia, un rendu local HTML limité est une étape beaucoup plus ciblée.

Source : [litehtml](https://github.com/litehtml/litehtml), qui se présente comme un moteur de mise en page léger et déconseille de le confondre avec un moteur de navigateur complet.
