# Outils réseau

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

Les commandes réseau du terminal sont décrites dans [TERMINAL.md](TERMINAL.md). Voir [PENTEST.md](PENTEST.md) pour l’application de diagnostic et [SSH.md](SSH.md) pour SSH/SFTP.
