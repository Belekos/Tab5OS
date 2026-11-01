# Port série

Réglages → Système → Série sélectionne **USB-C** ou **RS485**, puis le mode **Débogage** ou **Données**. Appliquer mémorise les paramètres en NVS uniquement lorsqu'ils changent. Le réglage initial est USB-C / Débogage. Aucune écriture périodique sur microSD.

- **USB-C** : périphérique USB Serial/JTAG intégré au P4, pour un câble de données relié à un ordinateur. Ouvrir son port série avec le logiciel du PC. Le débit et le format UART ne s'appliquent pas à cette liaison USB ; leurs champs sont désactivés.
- **RS485** : connecteur RS485 intégré de la Tab5, UART1, TX GPIO20, RX GPIO21, commande de direction GPIO34. Ne pas raccorder une liaison RS232 ou un UART TTL directement aux lignes A/B. Débits de 1 200 à 921 600 bauds ; formats 8N1, 8E1, 8O1, 8N2. La commande terminal accepte aussi un débit intermédiaire.
- **Débogage** : miroir des journaux applicatifs et de stdout/stderr vers le port choisi. L'UART0 de diagnostic reste disponible séparément. Sur le port choisi, la touche `m` demande les diagnostics mémoire uniquement en mode Débogage ; l'UART0 conserve cette commande dans les deux modes.
- **Données** : envoi/réception d'octets avec les commandes ci-dessous. Les journaux applicatifs et SDK ordinaires sont exclus de cette liaison. Les messages du boot ROM et d'une panne fatale peuvent encore apparaître au redémarrage : un protocole externe doit attendre le démarrage et prévoir son propre cadrage.

L'USB-A demeure l'hôte existant pour manettes, clavier, souris, stockage et RTL-SDR. Le service série n'installe aucun second hôte et ne reconfigure pas le contrôleur haute vitesse USB-A. Il n'utilise pas TinyUSB CDC : le périphérique matériel USB Serial/JTAG utilise le PHY pleine vitesse de l'USB-C.

## Terminal

```sh
serial                         # état, compteurs RX/TX, files et pertes
serial port usb                # ou rs485
serial mode data               # ou debug
serial baud 115200             # RS485 uniquement
serial frame 8N1               # RS485 : 8E1, 8O1, 8N2 aussi
serial ending crlf             # none, lf, crlf
serial send 'Bonjour'          # texte + fin de ligne configurée
serial write 'Bonjour'         # texte exact, sans fin ajoutée
cat /Documents/message.txt | serial write
serial write < /Documents/message.txt
serial read 4096 1000 > /Documents/reception.bin
serial listen 60000            # écoute 60 secondes ; Ctrl+C interrompt
serial clear                   # annule les files en attente
```

`read [octets [millisecondes]]` reçoit jusqu'au nombre indiqué pendant le délai indiqué : 8 192 octets / 1 000 ms par défaut. Un délai de 0 vide immédiatement les octets déjà disponibles. `listen [millisecondes]` continue jusqu'au délai (60 secondes par défaut, maximum une heure). Les redirections conservent les octets binaires ; l'affichage du Terminal sert au texte. `serial write` sans arguments lit son entrée standard. Les envois sont progressifs et interrompables ; si la file ne progresse plus pendant 10 secondes, la commande signale l'échec. Le succès signifie que les octets ont été acceptés par les files locales, pas qu'un équipement distant les a confirmés.

Le mode Données doit être choisi avant l'envoi. Les changements de paramètres sont refusés tant qu'un envoi applicatif reste en attente. `clear` ou « Annuler l'envoi » annule ce qui reste dans les files applicatives ; les octets déjà remis au pilote ou à l'équipement ne peuvent pas être retirés.

## Éditeur

Dans l’éditeur, **Affichage → Série** ouvre ou replie le volet à côté du texte. L’entrée est cochée lorsque le volet est visible. Il n’y a plus de bouton Série séparé dans la barre.

1. Choisissez USB-C ou RS485 et ses paramètres dans Réglages → Système → Série.
2. Ouvrez **Affichage → Série** dans l’éditeur. Si nécessaire, **Mode Données** active les échanges sans les journaux applicatifs.
3. **Envoyer** prend la sélection. Sans sélection, il transmet tout le document, y compris les passages hors écran. La ligne de source indique Sélection ou Document entier et le nombre d’octets.
4. **Écouter** reçoit un flux de texte dans le volet. **Arrêter** libère la réception. **Insérer** place le texte reçu au curseur ou remplace la sélection ; le document n’est pas modifié automatiquement.
5. **Effacer** vide seulement le texte reçu. **Annuler** interrompt la transmission de l’éditeur.

L’envoi copie un instantané en PSRAM, jusqu’à la limite de document de 2 Mio : modifier ensuite le document ne change pas les octets déjà préparés. Le contenu Markdown UTF-8 et ses fins de ligne restent inchangés ; aucune fin de ligne n’est ajoutée. Le presse-papiers est préservé. Les boutons inutilisables sont grisés : envoyer un document vide, arrêter une écoute inactive, annuler sans envoi ou insérer/effacer une réception vide. **Mode Données** disparaît une fois ce mode actif. Les boutons suivent la largeur du texte traduit ; une ligne trop longue passe à la ligne suivante, sans déborder du volet.

Réduire le volet conserve l’écoute ; le menu **Affichage** reste surligné pendant la réception ou l’envoi. **Fermer la partie droite** replie aussi ce volet, sans interrompre l’écoute. Quitter l’éditeur termine sa session de réception et annule son envoi restant. Un lecteur unique possède la réception : si le terminal écoute déjà, l’éditeur indique que la série est occupée. L’éditeur n’interrompt pas une transmission appartenant au terminal.

Le volet est destiné au texte : CRLF et CR sont affichés comme des sauts de ligne LF. Les octets non textuels ou les séquences UTF-8 invalides sont signalés et remplacés pour l’affichage. La réception conserve jusqu’à 16 Kio des données les plus récentes et signale une troncature. Pour garder un affichage réactif, le volet montre au maximum les 4 Kio les plus récents, sans couper un caractère UTF-8 ; **Affichage partiel** le signale. **Insérer** prend tout le tampon conservé, pas seulement cette fenêtre. Pour conserver des octets binaires, utilisez `serial read` ou `serial listen` avec une redirection dans le terminal. L’envoi de fichiers reste disponible par `serial write < /Documents/message.txt`.
