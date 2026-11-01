# RTL-SDR — Tab5OS 0.8.24

L’application reste un récepteur local USB. La version 0.8.24 conserve la clé ouverte pendant les changements de fréquence, de démodulation et de canal d’entrée : il n’est plus nécessaire d’arrêter puis de relancer la réception.

## Prise en main

1. Activer l’USB rapide de la tablette et redémarrer si ce réglage vient de changer. Brancher une clé compatible et son antenne.
2. Saisir la fréquence et choisir explicitement **kHz** ou **MHz**. Le clavier tactile fournit les chiffres et le point. Le changement d’unité convertit la valeur sans changer la fréquence.
3. Choisir **Spectre**, **WFM**, **AM** ou **NFM**, puis **Recevoir**, puis **Appliquer** pendant la réception. La fréquence peut ensuite être ajustée avec les boutons moins/plus et le **Pas (kHz)**. Une fréquence saisie s’applique par Entrée ou par le bouton, sans interrompre la session USB.
4. **Arrêter** libère la réception et sa sortie audio. Quitter l’application demande aussi cet arrêt. Le volume utilise le réglage global mémorisé par l’OS.

Les nouveaux préréglages sont des plages de réception indicatives : ondes longues, ondes moyennes, ondes courtes, FM, aviation, VHF et UHF. Ils ne verrouillent aucune fréquence, ne prétendent pas définir les règles d’un pays et ne garantissent pas que la clé possède la couverture correspondante. AM est une démodulation, pas une gamme de fréquences : les exemples ondes longues/moyennes sont exprimés correctement en kHz et l’aviation utilise également AM en VHF.

## Clés et entrée directe

L’ancienne limite logicielle universelle 24–1766 MHz a été retirée. L’interface accepte une fréquence positive représentable en hertz sur 32 bits ; le tuner détermine ensuite si elle peut être réglée. Un échec du pilote est affiché et la fréquence précédente est rétablie, ou la réception s’arrête si ce rétablissement échoue. L’acceptation d’une commande par le pilote ne démontre pas à elle seule la sensibilité ou l’absence d’alias de la clé.

L’onglet **Balayage** propose **Tuner**, **Direct I** et **Direct Q**. Une entrée directe exige un câblage/produit qui la supporte. Pour une RTL-SDR Blog V4, utiliser normalement **Tuner** : son circuit de conversion HF et son pilote prennent en charge le changement de gamme. Pour une Blog V3, la réception HF utilise généralement l’entrée directe Q. La bibliothèque intégrée contient les adaptations Blog V4/V4L ; aucun essai physique de ces modèles n’a été réalisé pour cette livraison.

Le débit I/Q est fixé à 240 kéchantillons/s. Les modes audio sont mono ; il n’y a pas de décodage RDS, stéréo, DAB, SSB, numérique ou de correcteur automatique de dérive AFC dans cette version. Les filtres AM et NFM sont distincts de WFM. Le filtre AM conserve le porteur centré, que l’ancien retrait de moyenne I/Q pouvait supprimer.

## Spectre et axes

Le tracé est rectangulaire indépendamment de l’arrondi choisi pour les boutons et les fenêtres. L’axe X affiche les fréquences absolues, avec trois repères. Les réglages permettent de choisir une largeur de 240, 120, 60 ou 30 kHz et un décalage par rapport au centre. Le décalage est borné à la bande effectivement reçue. Ce zoom est un agrandissement du spectre existant, pas une hausse du débit ou de la résolution FFT.

Les minima et maxima de Y sont réglables entre −120 et 0 **dBFS**, avec minimum strictement inférieur au maximum. Le calcul corrige le gain cohérent de la fenêtre Hann, relativement à un signal complexe pleine échelle. Il ne s’agit **pas de dBm**, ni d’une puissance RF calibrée. Le gain automatique du tuner peut modifier la hauteur des signaux. La FFT comporte 1024 points ; le tracé garde le maximum de chaque groupe de quatre bins, soit 256 colonnes. Le rafraîchissement est plafonné à environ 12,5 trames/s.

## Balayage, écoute et mémoires

Dans **Balayage**, sélectionner une plage suggérée ou saisir librement son début et sa fin en kHz. Le pas commun en haut de l’écran s’applique au balayage. La durée par pas est comprise entre 150 et 10 000 ms. Un lancement effectue un seul passage, limité à 20 000 positions ; les erreurs de fréquence arrêtent le balayage au lieu d’inventer une réception.

Le seuil en dBFS repère l’énergie autour du canal réglé sur au moins deux trames ; les bins centraux affectés par la composante continue sont exclus. Une détection indique donc un signal possible, sans identifier une station ni garantir qu’il sera intelligible. Les premières données après un changement sont écartées pour laisser le matériel se stabiliser. Le balayage est silencieux ; **Pause / reprendre** permet d’écouter la position courante, puis de continuer au canal suivant. L’option **Suspendre le balayage sur un signal** réalise cette pause automatiquement. **Écoute fixe** termine le balayage en conservant la fréquence et le mode audio choisis ; ce n’est pas un AFC.

**Détections** contient au maximum 128 entrées (fréquence, mode, entrée et niveau). Les doublons sont fusionnés. **Mémoriser** ajoute aussi une fréquence manuellement. Une simple sélection dans la liste ne change pas la réception ; **Recevoir la sélection** la rétablit explicitement. **Supprimer** retire l’entrée sélectionnée, **Effacer la liste** remet la liste de travail à zéro. L’astérisque indique des modifications non enregistrées.

**Enregistrer la liste** écrit `/System/SDR/detections.csv`, avec remplacement atomique et conservation de l’ancien fichier en cas d’échec. Il faut enregistrer après une suppression pour la conserver. Les données sont rechargées à l’ouverture ; un fichier tronqué ou invalide est rejeté entièrement. Aucun enregistrement automatique n’est effectué pendant chaque détection ou chaque changement de fréquence.

## Mémoire et interruptions

- Une tâche de réception possède la clé et exécute toutes les opérations USB. Les callbacks d’interface déposent seulement une commande ; ils n’effectuent pas de transfert USB.
- Les buffers FFT, I/Q (4096 octets) et PCM sont regroupés dans un bloc PSRAM réutilisé. La pile de tâche reste de 12 Kio ; l’admission réserve aussi une marge interne pour le pilote et les deux buffers USB DMA (512 + 4096 octets). La création de tâche échouée libère le bloc PSRAM.
- Aucun historique de spectres ou de signaux I/Q n’est accumulé. La file entre worker et interface est bornée ; si elle sature, le scan se met en pause plutôt que d’accumuler des allocations. La liste de 128 détections appartient à l’interface et est libérée à sa fermeture.
- Les changements de fréquence ne recréent ni la tâche, ni le pilote, ni les buffers. Le DSP audio est remis à zéro lors du changement. Les sorties audio sont réclamées/libérées avec le propriétaire SDR et ne sont pas reprises à un autre lecteur de force.
- La fin normale, l’absence de clé, l’échec de débit, le refus de réglage, l’échec USB et les échecs d’allocation suivent les chemins de nettoyage. Le pilote contrôle aussi la taille réelle retournée avant de copier un transfert et refuse une interface dont le seul endpoint appartient à un autre réglage alternatif.
- Limitation USB explicite : un transfert de contrôle EP0 bloqué ne peut pas être annulé par halt/flush dans le SDK utilisé. Son buffer DMA doit rester vivant jusqu’au callback de fin ou à la déconnexion. L’application indique de débrancher la clé ; elle ne réinitialise pas le bus partagé avec les disques et manettes. Il est dangereux de présenter cette rétention nécessaire comme une fuite corrigée par une libération forcée.

## Vérification

`tools/sdr-tests/run.py`, `run_ui.py` et `run_usb.py` exécutent les sources de production (seuls les includes matériels sont remplacés sur ordinateur). Les modèles et le DSP sont réels ; l’interface utilise LVGL et les composants de focus/édition/thème réels. Les frontières USB/RTOS/audio et stockage sont simulées pour injecter les pannes.

Les rapports `tools/verification/sdr-*-0824.json` détaillent les empreintes des sources, les essais et leurs limites : 2000 cycles worker, 2000 cycles adaptateur USB, 100 réouvertures UI, pénuries de mémoire, refus de fréquence, changement à chaud, scans bornés, pause/reprise, durée de vie DMA, clavier décimal, axes et sauvegardes échouées. Ils ne mesurent pas la SRAM restante d’une tablette ni la qualité RF ou audio d’une clé réelle. Le test matériel doit relever la mémoire avant réception, pendant réception/balayage, puis après arrêt et fermeture, en répétant ces actions et en débranchant la clé pendant la lecture.

## Références de conception

Les pratiques fréquence/pas, axes réglables, scan et mémoires se sont appuyées sur le [manuel officiel SDR++](https://www.sdrpp.org/manual.pdf). Le choix entre tuner et entrée directe suit les guides du fabricant [RTL-SDR Blog V4](https://www.rtl-sdr.com/v4/) et [Blog V3](https://www.rtl-sdr.com/rtl-sdr-blog-v-3-dongles-user-guide/). Ce sont des références ergonomiques et matérielles, pas une intégration du programme SDR++.
