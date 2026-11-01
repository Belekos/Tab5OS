# RTL-SDR

## Interface et réception

Le bandeau garde la fréquence, l’unité, le pas, la démodulation et les commandes de réception. **Recevoir** devient **Appliquer** pendant la réception ; ce bouton n’est actif que lorsqu’un réglage de réception a changé. **Arrêter** est grisé à l’arrêt et pendant la fermeture de la session USB. Le statut reste affiché sous les onglets. Les commandes **Balayer**, **Pause**/**Reprendre** et **Fixer** reflètent également l’état du balayage.

Les onglets sont **Spectre**, **Balayage**, **Détections** et **Réglages**. Les explications longues ont quitté les pages de travail ; elles restent dans l’aide et dans cette documentation. Les réglages de correction ppm, de désaccentuation, d’entrée et d’axes se trouvent ensemble dans **Réglages**. Les champs utilisent une largeur, une longueur et des caractères cohérents avec la valeur : décimales pour les fréquences, nombres entiers pour les ppm, niveaux, décalage et durée. Une nouvelle sélection de champ au clavier remplace la cible de saisie tactile précédente.

Le spectre conserve ses angles droits, indépendamment du thème. Ses repères Y sont répartis sur la hauteur réelle du graphique. L’application se recalcule lorsque la place disponible diminue, notamment avec le clavier ouvert ; les pages et la liste défilent dans leur propre surface. Les boutons de sélection, d’action et leurs états désactivés utilisent les couleurs du thème.

**Fixer** conserve la fréquence courante et arrête le balayage pour écouter. Ce n’est pas une correction automatique de dérive. Les minima/maxima Y restent exprimés en dBFS, sans prétendre mesurer une puissance RF calibrée. Les plages proposées demeurent modifiables ; le tuner détermine les fréquences réellement acceptées.

## Détections : 2 048 mémoires

La liste accepte **2 048 détections**. Elle présente **24 lignes par page** et crée seulement les widgets de cette page. Le compteur, la pagination et les actions restent hors de la liste défilante. Le changement de page sélectionne sa première ligne ; **Écouter** et **Supprimer** ne peuvent donc pas agir sur une sélection masquée dans une autre page. Sélectionner une ligne seul ne modifie pas la réception.

- **Écouter** rétablit la fréquence, le mode et l’entrée de la sélection.
- **Mémoriser** ajoute la fréquence courante ; les doublons sont fusionnés.
- **Supprimer** retire la ligne sélectionnée ; **Vider** efface la liste de travail.
- **Enregistrer** conserve la liste et les suppressions dans `/System/SDR/detections.csv`.

L’astérisque indique des changements non enregistrés. Il n’y a aucune écriture automatique à chaque détection. Le format CSV version 1 reste compatible avec les sauvegardes précédentes. Les enregistrements utilisent le remplacement atomique existant ; un fichier tronqué/invalide est rejeté entièrement. La capacité du lecteur de fichier a été augmentée avec celle de la liste, afin d’éviter une sauvegarde de grande liste impossible à recharger.

La liste de données occupe environ 16 Kio de PSRAM et appartient à l’interface, libérée à sa fermeture. Le tampon temporaire CSV de lecture est borné à 98 320 octets ; celui d’écriture est dimensionné sur la liste réelle (16 à 49 168 octets). Il existe seulement pendant une lecture ou une écriture ; aucune collection de 2 048 boutons n’est créée. La file de détections entre le récepteur et l’interface reste bornée et suspend le balayage en cas de saturation.

## USB et qualité FM

Le traitement FM comprend : retrait de composante continue après démodulation, filtre mono avant conversion à 48 kHz, désaccentuation 50/75 µs au choix et gain numérique RTL désactivé. La correction ppm reste réglable de −200 à +200. Un changement de fréquence/réglage réutilise la session USB ; il ne force pas une fermeture/réouverture de la clé.
