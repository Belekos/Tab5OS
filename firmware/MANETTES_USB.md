# Manettes USB - Tab5OS 0.8.24

## 8BitDo SN30 Pro

La SN30 Pro possède plusieurs modes : elle ne présente pas le même protocole dans chacun. Le modèle Bluetooth peut aussi fonctionner par câble. Le modèle nommé **SN30 Pro USB**, avec câble fixe, est différent.

| Modèle / mode | Connexion pour Tab5OS | Prise en charge dans cette version |
| --- | --- | --- |
| SN30 Pro Bluetooth, mode Android (`B` + `Start`) | Éteindre la manette, maintenir `B` + `Start`, puis brancher un câble USB de données à l’USB-A du Tab5 | Pilote générique HID, selon le descripteur présenté par la manette |
| SN30 Pro Bluetooth, mode Windows (`X` + `Start`) | Éteindre la manette, maintenir `X` + `Start`, puis brancher le câble | Ajout du transport USB XInput de type Xbox 360 |
| SN30 Pro USB, mode Windows | Brancher son câble à l’USB-A | Même transport XInput ajouté |
| Mode Switch (`Y` + `Start` sur le modèle Bluetooth) | — | Initialisation spécifique Switch non implémentée ; utiliser l’un des modes ci-dessus |
| Connexion Bluetooth sans câble | — | Aucun appairage Bluetooth de manette dans cette version |

Le manuel officiel indique les combinaisons de démarrage ; il ne constitue pas un essai de cette manette sur Tab5OS. Le mode Android est le choix à essayer avec le pilote HID. Le nouveau chemin XInput permet aussi la lecture des interfaces USB filaires `FF/5D/01` d’8BitDo et Microsoft. Les protocoles Xbox One et les récepteurs Xbox sans fil sont distincts et ne sont pas ajoutés.

HID et XInput restent disponibles simultanément : Tab5OS choisit le décodeur d'après l'interface USB annoncée, pas d'après le seul nom ou identifiant du fabricant. Aucun choix exclusif n'est imposé dans les réglages. Débrancher la manette avant de changer son mode puis la rebrancher avec un câble de données. Le mode HID est aussi appelé D-input dans la documentation 8BitDo.

La 0.8.22 corrige la recherche des descripteurs dans les périphériques composites : un descripteur HID absent ne peut plus être récupéré sur l'interface suivante ; un petit endpoint de statut ne masque plus un endpoint XInput valide ; seuls les réglages d'interface réellement revendiqués sont utilisés. Les interfaces clavier et souris restent analysées comme HID, le stockage et le SDR ne sont pas revendiqués.

La déconnexion conserve les données du périphérique jusqu'à l'acquittement de sa dernière interface. Une file d'événements pleine ne perd plus cet acquittement. Un manque de mémoire lors de la préparation d'une manette rend ses ressources et ne déclenche plus un arrêt fatal du pilote. Ces défauts sont reproduits avec les sources 0.8.21 ; cela ne permet pas d'attribuer avec certitude l'échec rencontré sur une manette physique à un seul d'entre eux.

Dans **Réglages → Interface → Manette**, la connexion et les commandes reçues sont visibles sans lancer de jeu. Le bouton **Configurer la manette** assigne les huit commandes A, B, Select, Start, X, Y, L et R. Relâcher les boutons entre chaque étape ; **Annuler** ou **Échap** abandonne sans changer le réglage. Un changement de page ou d’onglet annule aussi la capture. Dans un jeu, l’aide (`F1` ou bouton Aide) conserve son affichage et sa configuration. Le nom **8BitDo SN30 Pro** est affiché pour l’identifiant connu `2dc8:6001`, **8BitDo** pour les autres identifiants de ce constructeur. Les commandes actives s’affichent quand on appuie. **Configurer la manette** permet d’associer A, B, Select, Start et, dans les jeux concernés, X, Y, L, R. Le réglage est mémorisé par modèle et protocole.

Les vibrations, capteurs de mouvement et fonctions propres au fabricant ne sont pas pilotés. La disposition physique des lettres Nintendo/Xbox peut varier avec le mode : utiliser la configuration si A et B paraissent inversés.

## Corrections 0.8.24 et réception continue

Le chemin HID a été comparé avec les sources livrées en 0.8.15, 0.8.16, 0.8.17 et 0.8.18. Le parseur générique était inchangé. En 0.8.23, le contournement 8BitDo interprétait toutefois **tous** les paquets de neuf octets comme l'ancien format sans numéro : un rapport moderne valide pouvait alors devenir un autre bouton et des directions fantômes. La 0.8.24 donne priorité au descripteur quand le numéro et la longueur correspondent. L'ancien format reste disponible lorsque sa longueur ne correspond pas au rapport décrit.

Autre défaut établi : le callback ignorait un refus de soumission du transfert suivant. Après une erreur, l'appareil pouvait rester identifié sans aucun transfert actif. Le nouveau callback signale cet échec et le worker USB relance l'endpoint hors du callback, sans allocation ni boucle active ; les nouvelles tentatives échouées sont espacées d'une seconde. Une interruption de réception remet boutons et axes à zéro. Une entrée inactive n'est plus limitée par le délai de cinq secondes des requêtes de contrôle USB.

Ces défauts sont reproduits sur ordinateur. Ils **ne prouvent pas** à eux seuls la cause exacte sur une SN30 Pro physique. Les réglages affichent donc les identifiants USB, numéro d'interface, compteurs de rapports reçus/rejetés et erreurs/reprises USB, longueur et premiers octets du dernier rapport. Une photo permet de différencier absence de réception, décodage refusé et mauvaise association des boutons.

## Navigation dans l'OS et MicroPython

La manette est servie dès le démarrage, sans ouverture de F1 ni d'émulateur. Hors d'une vue de jeu native :

| Commande configurée | Action |
| --- | --- |
| Croix ou stick gauche | Déplacer le focus ; répétition après maintien |
| A / B | Valider / Retour |
| X ou Select | Menu contextuel |
| Y | Changer de zone de navigation |
| Start | Aide |
| L / R | Page précédente / suivante |

Dans un jeu ELF, ses commandes restent celles du jeu ; Guide continue d'ouvrir les contrôles F1. Pendant l'association des boutons dans les réglages, la navigation est suspendue pour éviter de quitter l'assistant au moment où l'on attribue B.

Les interfaces `tab5ui` ordinaires bénéficient de la navigation sans modification. Un script peut aussi lire directement l'état, indépendamment des autres lecteurs :

```python
import gamepad
from time import sleep_ms

while True:
    state = gamepad.state()
    if state['connected'] and state['buttons'] & gamepad.A:
        print(state['axes'])
    sleep_ms(30)
```

`gamepad.buttons()` renvoie le masque courant (sans clavier). `state()` fournit aussi les boutons bruts, quatre axes signés X/Y/RX/RY, le masque des axes présents, protocole et statistiques. Les valeurs vont de -32768 à 32767 ; Y positif vers le bas. Les gâchettes sont accessibles comme boutons selon le protocole. `gamepad.capture(True)` désactive les raccourcis de navigation uniquement quand l'interface Python est au premier plan, pour un jeu qui traite lui-même les commandes ; `capture(False)` les rétablit. La capture est réinitialisée au lancement de chaque script. Aucun de ces appels ne consomme les événements utilisés par l'OS ou les ELF.

## Corrections 0.8.23

L’hôte USB démarrait déjà dès le lancement de l’OS : F1 ouvrait l’affichage d’état, il ne démarrait pas le pilote. L’accès à cet état et à la configuration manquait dans les réglages ; il est maintenant présent.

Le démarrage XInput envoie maintenant la requête de fin d’initialisation utilisée par le pilote Linux xpad pour les contrôleurs Xbox 360 tiers (`C1/01`, valeur `0100`, réponse maximale de 20 octets). La lecture d’entrée démarre avant cette requête ; un contrôleur qui la refuse reste écouté. Cette compatibilité manquante est reproduite par le banc de test, mais ne prouve pas à elle seule la cause de l’absence de touches sur la manette de l’utilisateur.

Pour les anciens firmwares HID des SN30/SF30 Pro USB (`2dc8:6000`/`6001`), le paquet de neuf octets sans numéro de rapport est décodé explicitement, conformément au format documenté par SDL. Le premier octet contient des boutons : il ne doit pas être interprété comme un numéro de rapport 3 ou 4. Les rapports HID modernes restent décodés depuis leur descripteur. Les associations par défaut D-input tiennent compte de leurs numéros de boutons propres ; les associations enregistrées sont conservées.

L’état distingue une manette simplement identifiée d’une manette dont les commandes sont effectivement reçues : « Aucune commande reçue » ou « Commandes non reconnues » indique le point à vérifier. Ces messages sont aussi affichés dans F1. Le fonctionnement physique de chaque variante de firmware reste à confirmer sur Tab5.

## USB rapide et hubs

**USB rapide** reste un réglage global de l’hôte USB. Il doit être accessible même sans disque branché, notamment pour le RTL-SDR. La 0.8.17 avait placé ce choix dans la carte de stockage USB, masquée en l’absence de disque ; c’était une erreur d’emplacement, pas la suppression du pilote haute vitesse.

Le choix est conservé et prend effet après redémarrage. Le mode **USB compatible hubs** reste le réglage par défaut. Avec la pile ESP-IDF utilisée, les périphériques basse/pleine vitesse derrière un hub haute vitesse ne sont pas pris en charge lorsque l’hôte reste en haute vitesse. Pour vérifier une manette non reconnue, la brancher d’abord directement ; avec un hub, essayer le mode compatible puis redémarrer.

La vitesse USB n’ajoute pas le Bluetooth et n’est pas nécessaire au simple décodage d’une manette. Les boutons d’éjection et de montage d’un disque restent liés à la présence réelle du disque.

## Vérifications réalisées pour 0.8.24

- Même rapport HID moderne de neuf octets, via le callback de chaque version : 0.8.15 à 0.8.18 passent ; 0.8.23 renvoie le mauvais bouton et deux directions ; 0.8.24 passe. Ce rapport de protocole est construit pour isoler la régression, pas capturé sur une manette réelle.
- 46 095 assertions sur les fonctions C réelles du pilote avec doubles USB/RTOS : réception et resoumission, erreurs, annulation, initialisation XInput, pannes d'allocation, suspensions/reprises, 2 000 déconnexions. Aucune allocation suivie ni interface revendiquée retenue. La 0.8.23 échoue à cinq assertions sur le timeout et la resoumission.
- 4 069 assertions sur l'énumération composite et essais des vrais décodeurs.
- 20 048 assertions sur le callback `WriterUsb` et les API : rapports anciens et modernes, HID/XInput, 4 000 cycles, reprise temporisée après erreur et déconnexion.
- 59 assertions avec LVGL réel sur les réglages, associations et annulations.
- Navigation OS, répétition, absence de répétition de validation, changements de contexte, débordement du compteur de temps, pont Python et 4 000 lectures sans consommation ; décodage du vrai module Python.

Rapports : `tools/verification/gamepad-history-0824.json`, `gamepad-0824.json`, `gamepad-pipeline-0824.json`, `gamepad-settings-0824.json`, `gamepad-navigation-0824.json`.

Les tests sont exécutés sur ordinateur, avec le protocole et le code de production ; ils ne mesurent pas la SRAM réelle du Tab5. Les paquets sont des fixtures documentées, **pas des captures de la manette de l’utilisateur**. Le branchement physique, les hubs et les variantes de firmware restent à tester sur l’appareil.

Sur le Tab5 : brancher directement en HID, ouvrir **Réglages → Interface → Manette**, vérifier les commandes et les associer si nécessaire ; lancer ensuite le jeu. Refaire l’essai après déconnexion/reconnexion puis en XInput. Comparer le branchement direct et le hub avec le mode USB compatible hubs après redémarrage si nécessaire.

Sources : [manuel officiel SN30 Pro](https://download.8bitdo.com/Manual/Controller/SN30pro%2BSF30pro/SN30pro_Manual.pdf), [SN30 Pro Bluetooth](https://www.8bitdo.com/sn30-pro-g-classic-or-sn30-pro-sn/), [SN30 Pro USB](https://www.8bitdo.com/sn30-pro-usb-gamepad/), [FAQ des deux modèles](https://support.8bitdo.com/faq/sn30-pro-usb-controller.html), [protocole et identifiants dans le pilote Linux xpad](https://linux.googlesource.com/linux/kernel/git/torvalds/linux/+/42bb9b630c4c6c0964cddca98d9d30aa992826de/drivers/input/joystick/xpad.c), [limites de l’hôte USB ESP-IDF 5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/peripherals/usb_host.html).

Références de protocole pour ces corrections : [initialisation XInput dans Linux xpad](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c), [anciens et nouveaux rapports 8BitDo dans SDL](https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_8bitdo.c), [identifiants USB SDL](https://github.com/libsdl-org/SDL/blob/main/src/joystick/usb_ids.h).
