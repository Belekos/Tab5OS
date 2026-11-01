# Terminal

`help` classe les commandes ; `help commande` et `commande --help` détaillent leurs paramètres et donnent un exemple. Les commandes réseau de la version précédente sont conservées.

## Premiers exemples

```sh
pwd
ls -al /Documents
cd /USB
df
ping -c 3 -W 0.5 192.168.1.1
dns example.org
tcp -w 2 example.org 443
ports -w 0.35 192.168.1.1 22,80,443
headers -L https://example.org
wget -L -T 300 -O usb:/Documents/page.html https://example.org
curl -L -o sd:/Documents/page.html https://example.org
sha256 /USB/firmware.bin
```

Remplacer les hôtes et les chemins par ceux à utiliser. Le dossier de destination doit déjà exister. Un téléchargement préserve le fichier antérieur jusqu’à la réussite du transfert. Ajouter `--overwrite` pour remplacer un fichier existant ; une erreur ou une annulation supprime le fichier temporaire. HTTPS vérifie le certificat et le nom d’hôte ; une redirection de HTTPS vers HTTP est refusée.

## Chemins et édition

| Écriture | Destination |
|---|---|
| `/Documents/notes.txt` ou `sd:/Documents/notes.txt` | Carte SD |
| `/USB/Documents/notes.txt` ou `usb:/Documents/notes.txt` | Support USB monté |
| `notes.txt` | Dossier courant |
| `..` | Dossier parent |

Les chemins contenant des espaces doivent être entre guillemets. Les racines des supports ne peuvent pas être supprimées. Le terminal propose les mêmes actions d’édition que les autres champs : **Ctrl+A/C/X/V/Z/Y**, et **Ctrl+Maj+Z** pour rétablir. Les boutons correspondants portent des icônes et s’adaptent au champ actif. La sortie est sélectionnable mais reste en lecture seule.

**Haut/Bas** parcourt l’historique dans le champ de commande ; **Tab** complète. **Ctrl+C** copie une sélection, ou interrompt l’opération en l’absence de sélection. **Ctrl+.** interrompt également. **Ctrl+Entrée** exécute. La touche Meta peut remplacer Ctrl pour l’édition.

**Ctrl+L**, le bouton corbeille ou **clic droit → Effacer la sortie** effacent le contenu mémorisé de la sortie. Le dossier courant, les variables et l'historique des commandes restent disponibles. L'effacement ne stoppe pas une commande en cours : les nouvelles lignes continuent de s'afficher, sans réintroduire les anciennes au rafraîchissement. La commande `clear` reste disponible.

## Tubes, entrée et substitutions

```sh
ls /Documents | grep .txt
cat journal.txt | grep -i erreur | tail -n 20
seq 1 100 | head -n 5 | wc -l
grep -in erreur < journal.txt > erreurs.txt
echo "Le dossier courant est $(pwd)"
set dossier "$(dirname /Documents/notes.txt)"
echo `pwd`
```

`|` passe la sortie d'une commande à l'entrée de la suivante. `cat`, `head`, `tail`, `grep` et `wc` lisent cette entrée quand aucun fichier n'est précisé ; `-` la désigne explicitement parmi les fichiers. Sans tube ni redirection, cette entrée est vide : ces commandes n'attendent pas une saisie interactive. `< fichier` remplace l'entrée ; `>` et `>>` conservent leur mécanisme de remplacement ou d'ajout. Les diagnostics d'erreur restent dans le terminal et ne sont pas injectés dans le tube.

Les commandes d'un tube s'exécutent successivement. Le résultat intermédiaire est conservé en PSRAM, sans écrire un fichier temporaire sur SD. Le statut `$?` est celui de la dernière commande, comme pour un tube de shell habituel. Les changements de variables ou de dossier effectués dans un tube restent locaux à sa commande.

`$(commande)` et les accents graves capturent la sortie et retirent les sauts de ligne finaux. Les guillemets doubles préservent les espaces et les sauts de ligne internes ; sans guillemets, la sortie est découpée en arguments. Les apostrophes désactivent cette interprétation. Les substitutions peuvent contenir des tubes et d'autres substitutions. Leur dossier et leurs variables ne remplacent pas ceux du terminal. Une affectation seule, comme `resultat=$(false)`, garde le statut de la substitution.

Limites explicites : 16 commandes par tube ; 256 Kio entre deux commandes ; 4 niveaux de substitution et 8 Kio par substitution ; 512 arguments et 32 Kio d'arguments développés. Le cumul des appels de scripts, boucles et substitutions est également borné à six blocs d’exécution simultanés, afin de préserver la pile de la tâche. Un dépassement produit une erreur. Les tubes sont adaptés aux flux finis ; les producteurs infinis, les tâches en arrière-plan `&`, les groupes `(commandes)` et les redirections séparées `2>` ne sont pas implémentés. `Ctrl+C` sans sélection et le bouton Arrêter annulent l'exécution.

## Réactivité et mémoire

Le test d'annulation ne suspend plus la tâche à chaque fichier ou morceau de texte : il vérifie immédiatement l'annulation et cède le processeur à une cadence de cinq millisecondes. La sortie utilise un tampon circulaire de 32 Kio, sans déplacer tout le texte à chaque ligne quand il est plein. Pendant une commande, l'affichage est actualisé au plus deux fois par seconde ; le résultat final est affiché dès le passage suivant du service UI. Une sélection active reste intacte.

La sortie, son instantané d'affichage, les valeurs de variables, l'historique des commandes, les tubes, les substitutions capturées et le contenu conservé par `tail` utilisent explicitement la PSRAM. Les petites structures, chaînes de travail, arguments et la pile de la tâche utilisent encore l'allocateur du système. La pile n'est libérée que par la terminaison effective de la tâche ; le contexte est détruit après ce signal, jamais pendant une commande. Les données temporaires sont libérées en quittant leur portée ; fermer le terminal libère aussi l'historique et les variables.

`memory` affiche séparément la RAM interne libre, son plus grand bloc libre, la PSRAM libre et son plus grand bloc. Il permet de contrôler l'état réel de la tablette après plusieurs commandes.

## Référence complète

Dans les syntaxes ci-dessous, les crochets indiquent un paramètre facultatif et `...` plusieurs arguments. Ils ne sont pas à saisir littéralement. Dans les syntaxes de `test` et `ip`, `|` sépare des choix possibles : ce caractère ne doit pas être saisi pour choisir un argument.

### `help`

```text
help [COMMAND]
```

Liste les commandes ; ajoutez un nom pour ses paramètres et un exemple.

Exemple :

```sh
help wget
```

### `pwd`

```text
pwd
```

Affiche le dossier courant.

Exemple :

```sh
pwd
```

### `cd`

```text
cd [DIR]
```

Change de dossier. Sans paramètre : racine SD. .. désigne le parent ; /USB désigne le disque USB.

Exemple :

```sh
cd /USB/Documents
```

### `ls`

```text
ls [-a] [-l] [DIR]
```

Liste le dossier courant ou indiqué. -a inclut les fichiers cachés ; -l affiche le type et la taille.

Exemple :

```sh
ls -al /Documents
```

### `tree`

```text
tree [DIR]
```

Liste tous les chemins du dossier et de ses sous-dossiers, jusqu'à 16 niveaux.

Exemple :

```sh
tree /Documents
```

### `stat`

```text
stat FILE...
```

Affiche le type, la taille en octets et la date de modification de chaque chemin.

Exemple :

```sh
stat /Documents/notes.txt
```

### `du`

```text
du [DIR]
```

Calcule la taille des fichiers d'un dossier, sous-dossiers et fichiers cachés inclus.

Exemple :

```sh
du /Documents
```

### `cat`

```text
cat [FILE...]
```

Affiche les fichiers texte dans l'ordre indiqué. Sans fichier, lit l'entrée du tube ou de la redirection `<` ; `-` désigne cette entrée.

Exemple :

```sh
cat notes.txt
```

### `head`

```text
head [-n COUNT] [FILE...]
```

Affiche les premières lignes. -n : de 1 à 500 lignes ; 10 par défaut. Sans fichier, lit l'entrée du tube ou de la redirection `<`.

Exemple :

```sh
head -n 20 notes.txt
```

### `tail`

```text
tail [-n COUNT] [FILE...]
```

Affiche les dernières lignes. -n : de 1 à 500 lignes ; 10 par défaut. Maximum conservé : 64 Kio. Sans fichier, lit l'entrée du tube ou de la redirection `<`.

Exemple :

```sh
tail -n 30 journal.txt
```

### `wc`

```text
wc [-lwc] [FILE...]
```

Compte les sauts de ligne, mots et octets. -l, -w et -c choisissent les compteurs. Sans fichier, lit l'entrée du tube ou de la redirection `<`.

Exemple :

```sh
wc notes.txt
```

### `grep`

```text
grep [-i] [-n] TEXT [FILE...]
```

Cherche un texte littéral. -i ignore la casse ASCII ; -n affiche les numéros de ligne. Aucun résultat : code 1. Sans fichier, lit l'entrée du tube ou de la redirection `<`.

Exemple :

```sh
grep -in erreur journal.txt
```

### `find`

```text
find [DIR] [-name PATTERN]
```

Cherche récursivement jusqu'à 16 niveaux. -name filtre le nom ; placez les motifs entre apostrophes.

Exemple :

```sh
find /Documents -name '*.txt'
```

### `hexdump`

```text
hexdump [-n BYTES] [-s OFFSET] FILE
```

Affiche octets hexadécimaux et texte. -n : 1 à 65536 octets, 256 par défaut ; -s : position de départ en octets.

Exemple :

```sh
hexdump -n 64 -s 16 image.bin
```

### `cmp`

```text
cmp FILE1 FILE2
```

Compare deux fichiers, indique le premier octet différent et renvoie 1 s'ils diffèrent.

Exemple :

```sh
cmp original.bin copie.bin
```

### `mkdir`

```text
mkdir [-p] DIR...
```

Crée les dossiers. -p crée aussi les parents manquants et accepte un dossier déjà présent.

Exemple :

```sh
mkdir -p /Documents/Projet
```

### `rmdir`

```text
rmdir DIR...
```

Supprime uniquement des dossiers vides. La racine d'un support reste protégée.

Exemple :

```sh
rmdir /Documents/Vide
```

### `touch`

```text
touch FILE...
```

Crée un fichier vide s'il n'existe pas ; conserve le contenu d'un fichier existant.

Exemple :

```sh
touch notes.txt
```

### `cp`

```text
cp SOURCE DEST
```

Copie un fichier vers un fichier ou dossier. L'ancienne destination reste intacte si la copie échoue.

Exemple :

```sh
cp notes.txt /USB/Documents
```

### `mv`

```text
mv SOURCE DEST
```

Déplace ou renomme un fichier ou dossier sur le même support. Refuse d'écraser une destination existante.

Exemple :

```sh
mv brouillon.txt notes.txt
```

### `rm`

```text
rm [-r] PATH...
```

Supprime immédiatement les chemins. -r inclut le contenu des dossiers. La racine d'un support reste protégée.

Exemple :

```sh
rm brouillon.txt
```

### `basename`

```text
basename PATH
```

Affiche le dernier composant d'un chemin.

Exemple :

```sh
basename /Documents/notes.txt
```

### `dirname`

```text
dirname PATH
```

Affiche le dossier parent d'un chemin.

Exemple :

```sh
dirname /Documents/notes.txt
```

### `echo`

```text
echo [-n] TEXT...
```

Affiche les paramètres séparés par des espaces. -n omet le saut de ligne final.

Exemple :

```sh
echo 'Bonjour' > notes.txt
```

### `printf`

```text
printf FORMAT [TEXT...]
```

Affiche un format : %s insère un paramètre, %% un pourcentage, \n un saut de ligne et \t une tabulation.

Exemple :

```sh
printf '%s\n' Bonjour
```

### `seq`

```text
seq [START] END
```

Affiche une suite d'entiers croissants, un par ligne. Début : 1 par défaut ; maximum : 10000 valeurs.

Exemple :

```sh
seq 3 8
```

### `clear`

```text
clear
```

Efface l'affichage du terminal ; conserve l'historique et les variables.

Exemple :

```sh
clear
```

### `set`

```text
set NAME VALUE
```

Définit une variable ; nom=valeur est aussi accepté. Utilisez $nom ou ${nom}. Maximum : 32 variables.

Exemple :

```sh
set dossier /Documents
```

### `unset`

```text
unset NAME
```

Supprime une variable.

Exemple :

```sh
unset dossier
```

### `env`

```text
env
```

Affiche les variables et PWD, le dossier courant.

Exemple :

```sh
env
```

### `test`

```text
test -e PATH | -f PATH | -d PATH | TEXT = TEXT | TEXT != TEXT
```

Teste une existence (-e), un fichier (-f), un dossier (-d) ou deux textes. Résultat vrai : 0 ; faux : 1.

Exemple :

```sh
test -f notes.txt && cat notes.txt
```

### `true`

```text
true
```

Renvoie le code 0 ; utile dans les conditions de scripts.

Exemple :

```sh
true && echo oui
```

### `false`

```text
false
```

Renvoie le code 1 ; utile dans les conditions de scripts.

Exemple :

```sh
false || echo non
```

### `source`

```text
source FILE.sh [ARGS...]
```

Exécute un script dans le terminal courant. $0 à $9 contiennent son chemin et ses paramètres. Taille maximale : 32 Kio.

Exemple :

```sh
source /Scripts/sauvegarde.sh /Documents
```

### `sh`

```text
sh FILE.sh [ARGS...]
```

Exécute un script dans le terminal courant, comme source. Les changements de dossier et de variables persistent.

Exemple :

```sh
sh /Scripts/sauvegarde.sh /Documents
```

### `for`

```text
for NAME in VALUES; do COMMANDS; done
```

Répète les commandes pour chaque valeur. Maximum : 4 niveaux de scripts ou boucles et 10000 commandes par lancement.

Exemple :

```sh
for f in /Documents/*.txt; do wc "$f"; done
```

### `date`

```text
date
```

Affiche la date et l'heure de l'OS.

Exemple :

```sh
date
```

### `df`

```text
df [DIR]
```

Affiche l'espace libre et la capacité du support contenant le dossier ; dossier courant par défaut.

Exemple :

```sh
df /USB
```

### `memory`

```text
memory
```

Affiche séparément la mémoire libre et le plus grand bloc allouable en SRAM interne et en PSRAM.

Exemple :

```sh
memory
```

### `uptime`

```text
uptime
```

Affiche les secondes écoulées depuis le démarrage.

Exemple :

```sh
uptime
```

### `sleep`

```text
sleep SECONDS
```

Attend une durée de 0 à 3600 secondes ; les fractions utilisent un point. Ctrl+C interrompt l'attente.

Exemple :

```sh
sleep 0.5
```

### `sha256`

```text
sha256 FILE...
```

Calcule l'empreinte SHA-256 de chaque fichier, sur SD ou USB.

Exemple :

```sh
sha256 /USB/firmware.bin
```

### `wifi`

```text
wifi
```

Affiche le nom du réseau et l'adresse IPv4, ou l'état déconnecté.

Exemple :

```sh
wifi
```

### `netinfo`

```text
netinfo
```

Affiche le réseau Wi-Fi, IPv4, masque, passerelle, DNS et signal. Alias : ifconfig, ip addr, ip route.

Exemple :

```sh
netinfo
```

### `ifconfig`

```text
ifconfig
```

Alias de netinfo : affiche la configuration réseau actuelle.

Exemple :

```sh
ifconfig
```

### `ip`

```text
ip [addr|route]
```

Affiche IPv4, masque, passerelle et DNS. La modification des routes n'est pas disponible.

Exemple :

```sh
ip route
```

### `dns`

```text
dns HOST
```

Résout un nom DNS en IPv4, avec un délai maximal de 5 secondes. Alias : nslookup.

Exemple :

```sh
dns example.org
```

### `nslookup`

```text
nslookup HOST
```

Alias de dns : résout un nom DNS en IPv4.

Exemple :

```sh
nslookup example.org
```

### `ping`

```text
ping [-c COUNT] [-W SECONDS] HOST
```

Teste les réponses ICMP. -c : 1 à 100 requêtes, 4 par défaut ; -W : attente par requête, 1 seconde par défaut.

Exemple :

```sh
ping -c 3 -W 0.5 192.168.1.1
```

### `tcp`

```text
tcp [-w SECONDS] HOST PORT
```

Teste l'ouverture d'un port TCP sans envoyer de données. Port : 1 à 65535 ; -w : délai de connexion, 1.5 seconde par défaut.

Exemple :

```sh
tcp -w 2 example.org 443
```

### `nc`

```text
nc [-z] [-w SECONDS] HOST PORT
```

Alias de tcp. -z teste la connexion sans envoyer de données. Ce client ne crée pas de session interactive.

Exemple :

```sh
nc -z -w 2 192.168.1.1 80
```

### `ports`

```text
ports [-w SECONDS] HOST PORTS
```

Teste les ports d'un appareil. Liste : 22,80,443 ou 8000-8010 ; maximum 1024 ports. -w : délai par port, 1.5 seconde par défaut.

Exemple :

```sh
ports -w 0.35 192.168.1.1 22,80,443,8000-8010
```

### `headers`

```text
headers [-L] [-T SECONDS] URL
```

Affiche statut HTTP, certificat HTTPS vérifié et en-têtes utiles à l'audit. -L suit 5 redirections au maximum ; -T limite la durée totale de 1 à 3600 secondes, 120 par défaut.

Exemple :

```sh
headers -L https://example.org
```

### `wget`

```text
wget [-O FILE] [-L] [-T SECONDS] [--overwrite] URL
```

Télécharge sur SD ou USB. -O choisit le fichier ; sinon le nom vient de l'URL. -L suit les redirections ; -T limite la durée totale de 1 à 3600 secondes (120 par défaut). --overwrite remplace un fichier après réussite.

Exemple :

```sh
wget -L -O usb:/Documents/page.html https://example.org
```

### `curl`

```text
curl [-I] [-o FILE|-O] [-L] [-T SECONDS] [--overwrite] URL
```

Affiche une réponse HTTP (32 Kio maximum), ou l'enregistre avec -o fichier ou -O (nom de l'URL). -I demande les en-têtes ; -L suit les redirections ; -T limite la durée totale de 1 à 3600 secondes, 120 par défaut. HTTPS vérifie certificat et nom d'hôte.

Exemple :

```sh
curl -L -o sd:/Documents/page.html https://example.org
```

## Scripts et codes de retour

`;` sépare les commandes, `&&` continue si la précédente réussit et `||` si elle échoue. `>` remplace un fichier et `>>` ajoute du texte. Les variables s’utilisent avec `$nom`, `${nom}`, `$PWD` et `$?`. Les arguments de script sont accessibles par `$0` à `$9`.

`sh` et `source` emploient le même interpréteur que le terminal : hors tube ou substitution, les changements de dossier et de variables persistent. Dans un tube ou une substitution, ces changements restent locaux. Les tubes et substitutions sont décrits plus haut ; les tâches en arrière-plan `&` restent indisponibles.

| Code | Sens |
|---|---|
| 0 | Commande réussie ou condition vraie |
| 1 | Échec opérationnel, condition fausse ou aucun résultat selon la commande |
| 2 | Paramètres ou syntaxe invalides ; lire l’explication et la syntaxe affichées |
| 130 | Opération interrompue |

Un port TCP ouvert indique qu’une connexion a été acceptée ; cela ne prouve pas la sécurité ni le bon fonctionnement du service. Un dépassement de délai peut venir d’un filtrage ou d’un appareil inaccessible. Le DNS et les tests réseau utilisent IPv4 ; fournir les domaines internationaux sous leur forme DNS/Punycode.

L’application **Pentest** offre des opérations d’audit et un rapport exportable dans une interface dédiée. Les commandes réseau sont décrites dans [RESEAU_DIAGNOSTIC.md](RESEAU_DIAGNOSTIC.md).

## SSH, SFTP et tâches

help ssh décrit les commandes distantes, la confiance explicite dans la clé du serveur et les fichiers de clés ou de mot de passe. help sftp décrit la liste et les transferts de fichiers sans écrasement implicite. Les exemples complets, les formats de clés, les délais et les limites figurent dans [SSH.md](SSH.md). Cette version exécute des commandes distantes et leurs tubes ; elle ne fournit pas encore de terminal interactif VT/PTY.

	asks affiche un instantané réel des tâches FreeRTOS : identifiant, nom OS connu, état, priorité, minimum de pile libre en octets et compteur de temps du SDK. Le compteur est brut, pas un pourcentage CPU. memory complète cet instantané par l’état des ressources mémoire.

## Port série

`serial` affiche l’état, `serial mode data` active les échanges et `serial send "Bonjour"` envoie du texte. `serial write` conserve les octets et accepte une entrée par tube ou fichier ; `serial read` et `serial listen` reçoivent des données. Les réglages sont partagés avec Réglages → Système → Série. Voir [SERIE.md](SERIE.md) pour les ports, délais, fins de ligne et commandes complètes.
