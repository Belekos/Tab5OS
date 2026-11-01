# Réseau, téléchargements et Pentest — 0.8.23

Le **Terminal** et l’application ELF **Pentest** utilisent les mêmes opérations réseau supervisées par l'OS. Le Wi-Fi doit déjà être connecté. Pentest s'ouvre depuis **Applications ELF → Pentest** et demande son fichier ELF et son profil de lancement sur SD ; voir [PENTEST.md](PENTEST.md). L’aide intégrée décrit chaque paramètre ; la référence complète se trouve dans [TERMINAL.md](TERMINAL.md).

| Commande | Usage |
|---|---|
| `netinfo`, `ifconfig`, `ip addr`, `ip route` | Configuration actuelle : adresse, masque, passerelle, DNS et signal |
| `dns hôte`, `nslookup hôte` | Résolution IPv4 d’un nom |
| `ping -c 3 -W 0.5 hôte` | Trois requêtes ICMP, délai de 0,5 seconde par réponse |
| `tcp -w 2 hôte 443`, `nc -z -w 2 hôte 443` | Test de connexion TCP |
| `ports -w 0.35 hôte 22,80,443,8000-8010` | Test de ports explicitement choisis, jusqu’à 1024 ports distincts |
| `headers -L https://example.org` | En-têtes HTTP, validation TLS et observations sur les protections annoncées |
| `wget -L -O usb:/Documents/page.html URL` | Téléchargement vers USB, avec redirections |
| `curl -L -o sd:/Documents/page.html URL` | Téléchargement vers SD ; sans `-o`, sortie limitée à 32 Kio |

Pour les transferts HTTP, `-T` fixe une durée totale de 1 à 3600 secondes, 120 par défaut. Les attentes d’une connexion ou d’une lecture sont bornées séparément. Une annulation ou une expiration peut attendre la fin de la résolution DNS ou de la connexion en cours. La date du système doit être correcte pour HTTPS. Le certificat et le nom du serveur sont vérifiés ; un retour de HTTPS vers HTTP est refusé. Le fichier existant reste préservé si le transfert échoue ou est annulé.

## Application Pentest

L’accueil contient une application dédiée. Choisir une adresse IP ou un nom d’hôte, puis le type d’analyse :

- **Vue d’ensemble** : résolution DNS, quatre essais de ping et liste de ports TCP configurables.
- **DNS**, **Réponse réseau** ou **Ports TCP** : exécution de l’opération choisie.
- **HTTP et TLS** : saisir une URL ; le rapport indique le statut et les en-têtes, vérifie TLS pour HTTPS et relève notamment l’absence de HSTS, CSP ou X-Content-Type-Options. Ces observations doivent être interprétées selon le service ; elles ne prouvent pas seules une vulnérabilité.

**Ctrl+Entrée** lance, **Ctrl+.** arrête et **Ctrl+S** ouvre le choix du dossier d’export du rapport. **Ctrl+L**, la corbeille et **clic droit → Effacer la sortie** vident le rapport affiché sans arrêter une analyse en cours ni supprimer un export déjà enregistré. Le texte du rapport se sélectionne et se copie avec les commandes d’édition communes. Les rapports portent un nom unique `pentest-…txt` sur SD ou USB.

Un port ouvert correspond à une connexion acceptée. Un délai dépassé ne distingue pas un pare-feu d’un appareil inaccessible. L’application actuelle ne fournit pas de capture radio générale, d’injection Wi-Fi ni de moteur d’exploitation. Les opérations réseau restent annulables.

## Supervision réseau et VPN

**La 0.8.21 ne contient pas de client VPN. La supervision actuelle n'est pas
un passage réseau obligatoire avec protection contre les contournements.**
Les applications MicroPython utilisent les services HTTP/IRC de Tab5OS : aucun
module `socket` ou `network` n'est exporté. Les ABI ELF 1 à 4 n'exposent pas de
fonction réseau. Cependant, les applications natives ouvrent leurs propres
clients HTTP/TCP/UDP. Leurs verrous coordonnent les transferts et la mémoire ;
ils ne décident pas du trajet de chaque paquet. Le code ELF partage aussi la
mémoire de l'OS : son ABI n'est pas un bac à sable de sécurité.

La 0.8.19 appliquait aussi ce refus aux commandes internes de l'OS : après avoir bloqué les clients, la connexion, l'arrêt du Wi-Fi et le partage direct ne pouvaient plus acquérir le verrou. La 0.8.20 réserve une acquisition distincte à ces transitions internes, tout en attendant la libération des clients existants. Les recherches différées ou échouées publient désormais leur état au lieu de laisser une liste vide trompeuse. Si le pilote refuse l'arrêt du partage, le serveur se ferme et l'OS conserve la transition en attente jusqu'à l'arrêt réel ; il ne lance pas de connexion concurrente. Les tests reproduisent les trois échecs de la 0.8.19, puis contrôlent la correction avec le pilote simulé ; ils ne constituent pas un essai radio sur le Tab5.

### MicroLink v2

Le [projet](https://github.com/CamM2325/microlink) fournit un client Tailscale
avec WireGuard, DERP et découverte de pairs. Son auteur classe explicitement
le P4 parmi les cibles non testées. Les 85–116 Ko annoncés concernent la mémoire
statique SRAM ; les piles des tâches et les tampons PSRAM s'y ajoutent.

Dans le commit audité `216da3300f0493b0860247d43f7af5ce29df63a5`,
[`ml_wg_mgr.c`](https://github.com/CamM2325/microlink/blob/216da3300f0493b0860247d43f7af5ce29df63a5/components/microlink/src/ml_wg_mgr.c)
crée une interface `100.64.0.0/10` et des routes autorisées `/32` pour les pairs.
Aucune sélection de nœud de sortie ni route Internet par défaut n'a été trouvée
dans l'API et les sources examinées. **Le code fourni est donc une base pour
rejoindre un réseau Tailscale, pas une couverture Internet complète de Tab5OS.**
De plus, [`ml_tcp.c`](https://github.com/CamM2325/microlink/blob/216da3300f0493b0860247d43f7af5ce29df63a5/components/microlink/src/ml_tcp.c)
poursuit la connexion si l'attachement à l'adresse VPN échoue : cela ne répond
pas à une politique qui interdit tout repli direct.

Le défaut de réception décrit dans l'[issue 17](https://github.com/CamM2325/microlink/issues/17)
est encore présent dans la source auditée : appel direct à `ip_input` depuis
un autre contexte que la tâche lwIP. Des utilisateurs rapportent des plantages
sur S3 lors de transferts TCP. Cela demande une correction et une validation
P4/C6 ; l'étiquette « prêt pour la production » ne remplace pas ces essais.

### ESP32 NAT Router et composant WireGuard

[ESP32 NAT Router](https://github.com/martin-ger/esp32_nat_router) est un firmware
de routeur distinct. Son client WireGuard peut servir de référence sans remplacer
Tab5OS par ce firmware. La [documentation VPN](https://github.com/martin-ger/esp32_nat_router/wiki/WireGuard-VPN)
décrit le routage complet ou partiel, les DNS et la coupure des clients du point
d'accès lorsque le tunnel est indisponible ; elle précise une prise en charge
IPv4 et un seul pair. Ces protections des clients du point d'accès ne garantissent
pas, à elles seules, la protection des applications qui tournent sur le P4.

Le commit routeur `c44331dd475455242a8a8164ebca798774111de6` dépend du composant
[`esp_wireguard_idf_5.5.x`](https://github.com/martin-ger/esp_wireguard_idf_5.5.x),
audité au commit `02a24fad29365db09e0088df2f8c4414632fe35a`.
Ce composant crée une interface lwIP, peut la choisir comme route par défaut,
autorise un pair `0.0.0.0/0` et attache ses paquets WireGuard chiffrés à l'interface
physique. C'est une base de portage plus directe pour un VPN Internet Tab5OS.
Il faut conserver ses licences et adapter la construction de composant ESP-IDF
à Arduino. Aucune bibliothèque de ce dépôt n'a été incorporée dans cette livraison.

Le SDK P4 installé (`m5stack`, bibliothèques `3.3.9`, cible `esp32p4_es`)
active déjà IPv4, IPv6, `CONFIG_LWIP_IP_FORWARD` et `CONFIG_LWIP_IPV4_NAPT`.
L'absence de NAT n'est donc pas un obstacle démontré. Le rôle de client VPN
de la tablette ne nécessite d'ailleurs pas, à lui seul, un routeur NAT.

### Intégration nécessaire dans Tab5OS

L'OS doit posséder l'interface de tunnel et la politique de sortie au niveau IP.
Les applications peuvent garder leurs clients HTTP si le routage et le filtrage
sont imposés en dessous. Le portage doit notamment :

- gérer les clefs et le profil VPN dans les réglages de l'OS ;
- réserver la sortie physique aux paquets du tunnel et aux exceptions explicites
  nécessaires pour établir la connexion ;
- faire passer les DNS applicatifs par le tunnel, et protéger IPv6 ou le bloquer
  dans ce mode si le tunnel ne le prend pas en charge ;
- maintenir la politique lors du renouvellement DHCP et des reconnexions Wi-Fi ;
- bloquer le trafic protégé pendant une panne du tunnel, même pour un socket
  déjà ouvert ;
- valider ces règles par capture extérieure : démarrage, tunnel indisponible,
  changements de réseau, DNS, IPv6, flux audio/vidéo et téléchargements.

Une route par défaut seule ne suffit pas : une interface physique plus précise,
un changement DHCP ou un flux déjà établi peuvent réclamer un traitement séparé.
Le [gestionnaire du routeur](https://github.com/martin-ger/esp32_nat_router/blob/c44331dd475455242a8a8164ebca798774111de6/main/vpn_manager.c)
réapplique lui-même sa route VPN après les événements d'interface. Ces travaux
restent à intégrer et à mesurer sur le Tab5 ; aucun essai VPN matériel n'a été
effectué pour cette version.

## Tor

**La 0.8.21 ne contient pas de client Tor ni de réglage de proxy Tor.**
Un client Tor complet embarqué demanderait un portage séparé : le cœur Tor
utilise notamment Libevent, une pile cryptographique OpenSSL/NSS et zlib. La
mémoire disponible est partagée avec l’interface, les documents et les médias ;
aucun budget mémoire ni temps de démarrage satisfaisant n’a été mesuré ici.

L’architecture la plus raisonnable à explorer est un client Tor sur un routeur,
un PC ou un Raspberry Pi, accessible au Tab5 par SOCKS5. L’intégration Tab5OS
devrait utiliser le nom de destination dans SOCKS5 pour le faire résoudre par
Tor, puis conserver TLS jusqu’au site final. Il faut adapter chaque application
concernée, y compris les redirections HTTP, et empêcher un retour silencieux à
une connexion directe si le proxy est indisponible. Le proxy Tor ne transporte
pas les flux UDP via SOCKS5 ; il ne peut donc pas être présenté comme couvrant
automatiquement l’ensemble de l’OS. Le trajet entre la tablette et un proxy
sur le réseau local nécessite lui aussi une protection adaptée.

## Références consultées le 10 octobre 2026

- [ESP-Hosted : séparation des commandes radio et de la pile IP sur l’hôte](https://github.com/espressif/esp-hosted-mcu/blob/main/docs/getting-started-mcu.md)
- [ESP-NETIF sur ESP32-P4](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32p4/api-reference/network/esp_netif.html)
- [Composant WireGuard pour ESP-IDF : état et cibles déclarées](https://github.com/trombik/esp_wireguard)
- [Implémentation Arduino WireGuard basée sur lwIP](https://github.com/ciniml/WireGuard-ESP32-Arduino)
- [Architecture et dépendances du cœur Tor](https://tpo.pages.torproject.net/core/doc/tor/intro.html)
- [Tor : client partagé sur un réseau local](https://support.torproject.org/little-t-tor/getting-started/tor-client-central-server/)
- [Tor : protocole SOCKS, résolution des noms et absence de transport UDP](https://spec.torproject.org/socks-extensions.html)
- [Tor : contrôle des fuites DNS](https://support.torproject.org/little-t-tor/troubleshooting/check-for-leaks/)

Les tests logiciels utilisent des réponses réseau simulées et ne contactent
aucun appareil. Ils vérifient notamment les délais, l’annulation, la libération
des sockets, les réponses DNS tardives et la validation des paquets ICMP.
La syntaxe est également contrôlée avec les en-têtes et paramètres du P4.
Le fonctionnement radio réel et les latences doivent encore être vérifiés sur
le matériel.
