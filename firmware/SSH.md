# SSH et SFTP — Tab5OS 0.8.22

`ssh` exécute une commande sur un serveur SSH et restitue sa sortie, ses erreurs et son code de retour. `sftp` transfère un fichier ou liste un dossier distant. Les connexions passent par la supervision réseau de l'OS et se ferment avec **Ctrl+C**, le bouton d'arrêt du terminal, la fermeture du terminal ou la désactivation du réseau.

La commande distante est une opération ponctuelle : cette version ne fournit pas de terminal interactif PTY/VT100. Utilisez des commandes sans interface plein écran, ou des scripts sur le serveur. Par exemple, `uname`, `git status`, un script de sauvegarde ou `systemctl status --no-pager` conviennent ; `vim`, `top` interactif et une invite de mot de passe distante nécessitent un autre terminal.

## Préparer la confiance du serveur

1. Sur le serveur ou depuis une connexion déjà fiable, relever l'empreinte de sa clé hôte. Avec OpenSSH, par exemple : `ssh-keygen -lf /etc/ssh/ssh_host_ecdsa_key.pub -E sha256` ou la clé RSA utilisée par ce serveur.
2. Sur Tab5OS, exécuter `ssh keyscan serveur.example`. Cette commande affiche l'empreinte reçue sans envoyer d'identifiants ; elle ne prouve pas l'identité du serveur.
3. Comparer **toute** l'empreinte avec celle relevée indépendamment, puis enregistrer la valeur exacte :

```sh
ssh trust serveur.example SHA256:EMPREINTE_VERIFIEE
```

`SHA256:EMPREINTE_VERIFIEE` est un emplacement à remplacer, pas une empreinte utilisable. Une vraie empreinte contient 43 caractères base64 après `SHA256:`.

Les empreintes sont conservées dans la mémoire de configuration de la tablette, indépendamment de la carte SD, pour 32 couples hôte/port. Le port fait partie de l'identité : utiliser `-p 2222` dans `keyscan`, `trust` et les connexions si nécessaire. Si la clé change, l'authentification est bloquée. Après avoir vérifié la cause sur le serveur, `ssh forget serveur.example` retire uniquement l'ancienne entrée. Une commande `trust` n'écrase jamais silencieusement une autre empreinte.

## Authentification

La méthode recommandée est une clé privée **RSA ou ECDSA au format PEM**. Les clés Ed25519 et le conteneur `OPENSSH PRIVATE KEY` ne sont pas pris en charge par le profil mbedTLS livré. Une clé ECDSA compatible peut être créée sur un PC avec :

```sh
ssh-keygen -t ecdsa -b 256 -m PEM -f id_tab5
```

Installer la clé publique sur le serveur, puis placer la clé privée et son fichier public compagnon `id_tab5.pub` dans le même dossier sur la tablette. Ce compagnon au format OpenSSH est nécessaire aux clés ECDSA ; les clés RSA PEM peuvent aussi être utilisées seules. Exemple :

```sh
ssh -i sd:/Keys/id_tab5 user@serveur.example 'uname -a'
ssh -i usb:/Keys/id_tab5 user@serveur.example 'git -C /home/user/projet status --short'
```

`--passphrase-file FILE` fournit la phrase de chiffrement d'une clé PEM. Pour un serveur autorisant les mots de passe, `--password-file FILE` remplace `-i KEY` :

```sh
ssh --password-file sd:/Keys/server.secret user@serveur.example 'uname -a'
```

Le contenu du fichier secret n'entre ni dans la ligne de commande, ni dans l'historique, ni dans l'affichage ; son tampon est effacé avant libération. Les fins de ligne CR/LF finales sont retirées. Limites : 16 Kio pour la clé privée, 1 Kio pour un secret. Aucun argument `--password MOT_DE_PASSE` n'est accepté.

Ces fichiers restent lisibles sur leur support. Le transfert SSH les protège sur le réseau, pas au repos. Un fichier de phrase secrète placé à côté de la clé chiffrée n'offre pas une protection équivalente à une phrase saisie et conservée séparément. Cette version n'a pas d'invite secrète ni de liaison automatique au coffre.

## Tubes, délais et résultats

```sh
cat sd:/Documents/notes.txt | ssh -i sd:/Keys/id_tab5 user@serveur.example 'cat > /home/user/notes.txt'
ssh -i sd:/Keys/id_tab5 user@serveur.example 'printf "resultat\n"' | grep resultat
ssh --timeout 300 -i sd:/Keys/id_tab5 user@serveur.example '/home/user/traitement.sh'
```

Entourer la commande distante d'apostrophes évite son expansion prématurée par le terminal local. Les octets d'entrée sont envoyés dans stdin ; stdout peut alimenter un tube ou une redirection. stderr reste dans le terminal. Le statut retourné par la commande distante est conservé ; l'annulation locale renvoie 130.

`--timeout SECONDS` règle le délai sans données, de 1 à 3600 secondes, avec 30 secondes par défaut. La résolution DNS et la connexion TCP ont une limite de 5 secondes ; la négociation et l'authentification, 20 secondes. Un serveur qui ne produit rien pendant le délai choisi est interrompu même si son programme travaille encore. L'arrêt ferme le canal ; le comportement du programme distant après déconnexion dépend du serveur et de ce programme.

Les adresses IPv4 et noms DNS sont pris en charge ; les noms internationaux doivent utiliser leur forme Punycode. IPv6, tunnel SSH, agent SSH, authentification interactive PAM/MFA, SCP et émulation ANSI ne sont pas proposés par ces commandes.

## Transferts SFTP

```sh
sftp ls -i sd:/Keys/id_tab5 user@serveur.example /home/user
sftp get -i sd:/Keys/id_tab5 user@serveur.example /home/user/document.txt sd:/Documents/document.txt
sftp put -i sd:/Keys/id_tab5 user@serveur.example /home/user/document.txt sd:/Documents/document.txt
```

L'ordre est toujours **chemin distant, chemin local**. `get` reçoit, `put` envoie. Les mêmes options `-p`, `-i`, `--password-file`, `--passphrase-file` et `--timeout` s'appliquent. Le dossier destination doit déjà exister. Les fichiers existants sont refusés ; choisir un autre nom pour conserver l'original.

Le transfert utilise un fichier temporaire portant le suffixe `.tab5-sftp-part`, puis le renomme après succès. Une réception annulée efface son fichier temporaire local. Une interruption de liaison ou une annulation pendant l'envoi peut laisser un fichier temporaire distant : il reste explicitement incomplet et peut être supprimé sur le serveur avant de recommencer. Les fichiers temporaires préexistants ne sont pas écrasés. Le transfert ne charge jamais tout le fichier en RAM.

## Dépendance, mémoire et validation

Les sources de **libssh2 1.11.2_DEV**, licence BSD-3-Clause, sont incluses dans `src/tab5ssh`, avec `COPYING` et `UPSTREAM.json`. Il s'agit du snapshot officiel épinglé au commit `18aebe08962c1878d84c373ad85caff25ec67771`, pas d'une version stable 1.11.2 publiée. Il inclut des corrections de mémoire et de vérification des paquets absentes de la dernière version publiée 1.11.1. Le choix et les commits concernés sont décrits dans `MBEDTLS_BACKPORTS.md`. Le moteur cryptographique est mbedTLS fourni par le SDK ESP32-P4 ; aucun chiffrement propre à Tab5OS n'est ajouté.

Les adaptations locales concernent la configuration Arduino/lwIP, le profil de mémoire et la libération d'un tampon de préférences KEX lorsque l'allocation suivante échoue. Cette fuite a été reproduite par injection d'échec mémoire ; elle est corrigée sans changer les algorithmes. Le conteneur RSA est accepté avec une signature SHA-2 tout en maintenant désactivées les signatures RSA/SHA-1. Les méthodes proposées sont ECDH P-256 ou DH groupe14/SHA-256, clés hôtes ECDSA P-256 ou RSA/SHA-2, AES-CTR et HMAC-SHA-256, avec strict KEX lorsque le serveur le prend en charge.

Les allocations de session sont plafonnées à 512 Kio en PSRAM sans repli SRAM. Fenêtre de canal : 64 Kio ; paquet : 16 Kio ; tampons applicatifs : 2 Kio pour SSH, 4 Kio pour SFTP. Le gestionnaire de ressources admet en plus le budget cryptographique réel du SDK. Les allocations internes à mbedTLS utilisent l'allocateur commun installé par l'OS ; la commande ne change pas globalement cet allocateur.

La pile du terminal n'est pas agrandie pour toutes les commandes. SSH est refusé si le minimum de pile libre signalé par FreeRTOS est inférieur à 8 Kio, notamment sous des scripts ou substitutions très imbriqués. Relancer directement la commande contourne cette profondeur, sans supprimer la protection. `tasks` affiche les mesures réelles de pile ; `memory` affiche les espaces et les plus grands blocs disponibles.

Sources officielles : <https://libssh2.org/>, <https://github.com/libssh2/libssh2>, <https://libssh2.org/license.html>.

## English usage summary

`ssh` executes one remote command; it is not a PTY/VT100 emulator. Verify the complete host fingerprint independently, record it with `ssh trust HOST SHA256:...`, then use `ssh -i sd:/Keys/id_tab5 USER@HOST 'command'`. A changed host key is rejected before credentials are sent. `ssh forget HOST` removes the old pin after the server change has been checked. Hostname and port identify each pin separately.

Authentication accepts RSA/ECDSA PEM private keys or `--password-file FILE`; encrypted PEM keys may use `--passphrase-file FILE`. ECDSA requires the matching OpenSSH public-key file beside the private key, with `.pub` appended to its name. Secret file contents are not echoed or added to history, but files are not automatically encrypted on storage. Ed25519/OpenSSH private-key containers and interactive authentication are unsupported. Pipes transport stdin/stdout; stderr remains separate. `--timeout 1..3600` sets the idle timeout in seconds, default 30.

`sftp get|put [OPTIONS] USER@HOST REMOTE LOCAL` transfers one file; `sftp ls [OPTIONS] USER@HOST REMOTE` lists a directory. Existing files are never implicitly overwritten. Transfers stream through bounded buffers and use `.tab5-sftp-part` temporary files. An interrupted upload can leave a remote temporary file that must be removed before retrying. Close or Ctrl+C cancels the operation.
