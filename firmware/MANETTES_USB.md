# Manettes USB

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

Dans **Réglages → Interface → Manette**, la connexion et les commandes reçues sont visibles sans lancer de jeu. Le bouton **Configurer la manette** assigne les huit commandes A, B, Select, Start, X, Y, L et R. Relâcher les boutons entre chaque étape ; **Annuler** ou **Échap** abandonne sans changer le réglage. Un changement de page ou d’onglet annule aussi la capture. Dans un jeu, l’aide (`F1` ou bouton Aide) conserve son affichage et sa configuration. Le nom **8BitDo SN30 Pro** est affiché pour l’identifiant connu `2dc8:6001`, **8BitDo** pour les autres identifiants de ce constructeur. Les commandes actives s’affichent quand on appuie. **Configurer la manette** permet d’associer A, B, Select, Start et, dans les jeux concernés, X, Y, L, R. Le réglage est mémorisé par modèle et protocole.

Les vibrations, capteurs de mouvement et fonctions propres au fabricant ne sont pas pilotés. La disposition physique des lettres Nintendo/Xbox peut varier avec le mode : utiliser la configuration si A et B paraissent inversés.

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

## USB rapide et hubs

**USB rapide** reste un réglage global de l’hôte USB. Il doit être accessible même sans disque branché, notamment pour le RTL-SDR. La 0.8.17 avait placé ce choix dans la carte de stockage USB, masquée en l’absence de disque ; c’était une erreur d’emplacement, pas la suppression du pilote haute vitesse.

Le choix est conservé et prend effet après redémarrage. Le mode **USB compatible hubs** reste le réglage par défaut. Avec la pile ESP-IDF utilisée, les périphériques basse/pleine vitesse derrière un hub haute vitesse ne sont pas pris en charge lorsque l’hôte reste en haute vitesse. Pour vérifier une manette non reconnue, la brancher d’abord directement ; avec un hub, essayer le mode compatible puis redémarrer.

La vitesse USB n’ajoute pas le Bluetooth et n’est pas nécessaire au simple décodage d’une manette. Les boutons d’éjection et de montage d’un disque restent liés à la présence réelle du disque.
