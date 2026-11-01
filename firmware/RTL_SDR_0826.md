# RTL-SDR — Tab5OS 0.8.26

## Interface et réception

Le bandeau garde la fréquence, l’unité, le pas, la démodulation et les commandes de réception. **Recevoir** devient **Appliquer** pendant la réception ; ce bouton n’est actif que lorsqu’un réglage de réception a changé. **Arrêter** est grisé à l’arrêt et pendant la fermeture de la session USB. Le statut reste affiché sous les onglets. Les commandes **Balayer**, **Pause**/**Reprendre** et **Fixer** reflètent également l’état du balayage.

Les onglets sont **Spectre**, **Balayage**, **Détections** et **Réglages**. Les explications longues ont quitté les pages de travail ; elles restent dans l’aide et dans cette documentation. Les réglages de correction ppm, de désaccentuation, d’entrée et d’axes se trouvent ensemble dans **Réglages**. Les champs utilisent une largeur, une longueur et des caractères cohérents avec la valeur : décimales pour les fréquences, nombres entiers pour les ppm, niveaux, décalage et durée. Une nouvelle sélection de champ au clavier remplace la cible de saisie tactile précédente.

Le spectre conserve ses angles droits, indépendamment du thème. Ses repères Y sont répartis sur la hauteur réelle du graphique. L’application se recalcule lorsque la place disponible diminue, notamment avec le clavier ouvert ; les pages et la liste défilent dans leur propre surface. Les boutons de sélection, d’action et leurs états désactivés utilisent les couleurs du thème.

**Fixer** conserve la fréquence courante et arrête le balayage pour écouter. Ce n’est pas une correction automatique de dérive. Les minima/maxima Y restent exprimés en dBFS, sans prétendre mesurer une puissance RF calibrée. Les plages proposées demeurent modifiables ; le tuner détermine les fréquences réellement acceptées.

## Détections : 2 048 mémoires

La liste accepte **2 048 détections**, contre 128 auparavant. Elle présente **24 lignes par page** et crée seulement les widgets de cette page. Le compteur, la pagination et les actions restent hors de la liste défilante. Le changement de page sélectionne sa première ligne ; **Écouter** et **Supprimer** ne peuvent donc pas agir sur une sélection masquée dans une autre page. Sélectionner une ligne seul ne modifie pas la réception.

- **Écouter** rétablit la fréquence, le mode et l’entrée de la sélection.
- **Mémoriser** ajoute la fréquence courante ; les doublons sont fusionnés.
- **Supprimer** retire la ligne sélectionnée ; **Vider** efface la liste de travail.
- **Enregistrer** conserve la liste et les suppressions dans `/System/SDR/detections.csv`.

L’astérisque indique des changements non enregistrés. Il n’y a aucune écriture automatique à chaque détection. Le format CSV version 1 reste compatible avec les sauvegardes précédentes. Les enregistrements utilisent le remplacement atomique existant ; un fichier tronqué/invalide est rejeté entièrement. La capacité du lecteur de fichier a été augmentée avec celle de la liste, afin d’éviter une sauvegarde de grande liste impossible à recharger.

La liste de données occupe environ 16 Kio de PSRAM et appartient à l’interface, libérée à sa fermeture. Le tampon temporaire CSV de lecture est borné à 98 320 octets ; celui d’écriture est dimensionné sur la liste réelle (16 à 49 168 octets). Il existe seulement pendant une lecture ou une écriture ; aucune collection de 2 048 boutons n’est créée. La file de détections entre le récepteur et l’interface reste bornée et suspend le balayage en cas de saturation.

## USB et qualité FM

Le DSP FM corrigé en 0.8.25 est conservé : retrait de composante continue après démodulation, filtre mono avant conversion à 48 kHz, désaccentuation 50/75 µs au choix et gain numérique RTL désactivé. La correction ppm reste réglable de −200 à +200. Un changement de fréquence/réglage réutilise la session USB ; il ne force pas une fermeture/réouverture de la clé.

L’adaptateur USB n’ajoute plus une attente d’un tick après que le callback de transfert a déjà annoncé sa fin. Il conserve cette attente tant que le DMA est en cours, et le worker conserve son point de coopération. Le changement ne modifie ni la durée de vie des buffers DMA, ni l’attente obligatoire de fin d’EP0, ni la gestion de débranchement/annulation. Le SDK cible est configuré à 1 000 ticks/s : une attente inutile de 1 ms par bloc terminé a ainsi été retirée. Cela ne constitue pas une mesure de débit ou de réception sur tablette réelle.

## Pourquoi le récepteur reste natif

L’API ELF actuelle propose un framebuffer et un formulaire/console générique. Elle ne permet pas de construire cette interface LVGL complète depuis un ELF. Déplacer le DSP et l’USB vers un ELF sans mesurer les accès au code, la bande passante et l’ordonnancement ne permettrait pas de garantir l’absence d’impact demandée. Un petit ELF qui ouvrirait simplement la même application native serait seulement un lanceur, pas une externalisation réelle ; il n’est pas ajouté comme faux portage.

Une évolution possible est un service radio natif conservant la boucle I/Q, avec une API graphique générique suffisante pour un véritable frontal ELF. Il faudra mesurer la mémoire, le coût d’échange des spectres et la latence avant de remplacer l’application actuelle. En 0.8.26, le chemin audio/radio demeure donc natif.

## RDS et RDS2

Cette version **ne décode ni RDS ni RDS2**. Le filtrage qui empêche le RDS de parasiter le son FM n’est pas un décodage de données.

Le [RDS Forum](https://www.rds.org.uk/pages/rds2.html) décrit les flux supplémentaires, groupes C et services propres à RDS2. Son [schéma des sous-porteuses](https://www.rds.org.uk/2010/pdf/R15_031_2.pdf) situe le RDS classique à 57 kHz et les trois flux supplémentaires à 66,5, 71,25 et 76 kHz. Un simple décodeur du nom de station à 57 kHz ne doit pas être présenté comme RDS2.

Dans Tab5OS, un décodeur doit extraire les données du multiplex FM **avant** le filtre sonore et la réduction à 48 kHz. Il faut ajouter synchronisation de porteuse et d’horloge, décodage des blocs et contrôle d’erreur, puis les groupes/services RDS2. La cadence de 240 kHz rend l’extraction numérique du multiplex envisageable ; elle ne démontre pas la qualité RF des sous-porteuses ni la charge CPU réelle. Il manque encore les décodeurs et leurs validations sur captures réelles, notamment signal faible et perte de synchronisation. Aucun nom de station ou logo simulé n’est affiché.

## ADS-B local et exemple tab5-devos

La réception ADS-B à 1 090 MHz est une piste crédible pour le matériel, mais **elle n’est pas disponible dans cette version**. La référence [dump1090](https://github.com/antirez/dump1090/blob/master/README.md) exploite un flux I/Q 8 bits à 2 Méchantillons/s, soit **4 000 000 octets/s**. Avec nos blocs de 4 096 octets, il faut en traiter environ **977 par seconde**, soit un bloc toutes les **1,024 ms**.

Le chemin actuel n’a qu’un transfert USB en vol, puis copie, calcule et coopère avec l’OS avant de soumettre le suivant. Même après suppression de l’attente inutile en fin de transfert, le point de coopération de 1 ms par bloc laisse une marge théorique minime pour les transferts et le décodage à ce débit. On ne peut pas activer simplement 2 Méch/s et promettre une carte d’avions fiable.

Il faut un pipeline de plusieurs transferts DMA, une file bornée, la mesure des pertes de blocs et de la charge CPU, puis un décodeur Mode S avec validation CRC, identification et positions CPR. Des captures connues doivent vérifier le décodage et l’absence de fausses positions avant une carte/radar. Les contraintes de mémoire DMA interne et de coexistence avec les autres périphériques USB doivent aussi être mesurées sur P4. Une clé et une antenne capables de recevoir 1 090 MHz restent nécessaires.

L’exemple fourni par l’utilisateur, [tab5-devos — devos_adsb.c](https://github.com/domgrimm/tab5-devos/blob/main/components/devos_adsb/devos_adsb.c), a été inspecté : il télécharge périodiquement `aircraft.json` et `receiver.json` auprès d’un récepteur réseau dump1090/readsb. Son application affiche les données déjà décodées ; ce module ne reçoit pas les I/Q d’une clé USB branchée à la tablette. Cette architecture serait une alternative plus légère avec un récepteur externe, mais elle ne remplit pas à elle seule la demande de clé RTL-SDR locale.

## Vérification et limites

Le test UI exécute le code de production avec LVGL, Focus, TextInput et UiStyle réels. Il couvre les pages de 24 entrées, la dernière entrée 2 048, les états Recevoir/Appliquer/Arrêter, les champs numériques, le changement d’unité avec saisie en attente, la sélection visible, le rechargement d’une grande sauvegarde et les hauteurs réduites par le clavier. Il effectue également 100 ouvertures/fermetures. Le test DSP conserve les signaux FM synthétiques et les mesures introduits en 0.8.25.
