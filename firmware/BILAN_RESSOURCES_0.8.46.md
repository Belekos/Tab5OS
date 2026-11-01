# Ressources et validation — Tab5OS 0.8.46

Référence : 0.8.45 livrée, archive authentifiée. Les chiffres du firmware proviennent du compilateur/éditeur de liens P4 réel. Les chiffres des scénarios hôte décrivent seulement les appels et allocations instrumentés. **Aucune mesure physique de heap libre, fragmentation, CPU, FPS, autonomie, temps de démarrage ou latence n'a été réalisée.**

## Firmware lié

| Mesure, en octets | 0.8.45 | 0.8.46 | Évolution |
|---|---:|---:|---:|
| Image OTA applicative | 6 067 488 | 6 071 360 | +3 872 |
| Partition OTA | 6 553 600 | 6 553 600 | inchangée |
| Marge dans la partition | 486 112 | 482 240 | −3 872 |
| Données + BSS internes | 93 268 | 93 372 | +104 |
| `.dram0.data` | 31 364 | 31 428 | +64 |
| `.dram0.bss` | 45 220 | 44 052 | −1 168 |
| `.dram1.bss` | 16 684 | 17 892 | +1 208 |
| `.iram0.text` | 98 494 | 98 610 | +116 |
| TCM code / données | 794 / 64 | 794 / 64 | inchangés |
| RTC noinit | 1 200 | 1 200 | inchangé |

L'image utilise **92,64 %** de sa partition ; il reste **470,94 Kio**. Les lignes DRAM correspondent à des sections liées : le déplacement entre banques n'est pas une économie de heap. Le nouvel état du garde réseau représente 48 octets dans son objet cible ; il n'ajoute ni pile ni worker. Les tailles de piles existantes et les marges d'admission sont conservées. La trame locale compilée de l'entrée audio passe de 48 à 320 octets dans sa pile de 16 Kio, à la suite de l'attente de commande ; ce n'est pas une mesure du minimum de pile en fonctionnement.

« Application / partition OTA » compare la taille du programme à sa capacité de stockage en Flash, **pas** sa consommation RAM. Ici 6 071 360 octets valent environ 5,79 Mio et la partition 6,25 Mio. NVS, OTA, SPIFFS, coredump et les deux emplacements applicatifs restent aux mêmes offsets/tailles. Aucun repartitionnement ni migration destructive.

## Scénarios exécutés sur les sources

| Scénario | Avant | Après | Portée |
|---|---:|---:|---|
| Radio établie + admission d'une requête Cartes | refus | autorisation | Garde de production, threads/FreeRTOS adaptés ; mémoire d'admission conservée |
| 40 mises à jour de toute l'image : plages passées à la synchronisation cache | 145 843 200 o | 73 785 600 o | −49,4 % d'adresses parcourues ; pas de débit PSRAM physique mesuré |
| Début de trait, 640×360, huit calques : copie | 7 373 168 o | 921 968 o | Métadonnées hôte comprises ; seul calque modifié copié |
| Même taille, un calque : copie | 921 968 o | 921 968 o | Copie du calque actif conservée |
| Initialisation de couverture au début du trait | 230 400 o | 113 o | Puis initialisation des blocs réellement touchés ; premier petit tampon : 512 o |
| Reconstruction catalogue sur 4 000 clics d'onglet Média déjà actif | 4 000 | 0 | Fonctions UI réelles avec frontières contrôlées |
| Faux refus Wi-Fi dans la matrice d'import | 24 | 0 | 512 combinaisons audio/réseau/SD ; pas de panne radio physique reproduite |
| Attente d'Appliquer série si worker momentanément bloqué | 2 000 ms simulées | 0 attente UI | Demande suivie jusqu'au vrai résultat ; NVS en échec distinct d'une restauration du port |

Le scénario audio final constate également la suppression des réveils à un tick du récepteur avant la fin de HTTP et du décodeur local en pause. Les valeurs dépendent de l'ordonnancement hôte ; aucun pourcentage de CPU physique n'en est déduit. La suppression des pauses fixes après les tuiles ne change pas les limites de cadence imposées par les fournisseurs.

## Compilation et tests réellement exécutés

Compilation complète Linux/GCC : **1 423 objets**, SDK Arduino M5Stack 3.3.9 / ESP-IDF 5.5.4, GCC RISC-V 14.2. Contrôle des sources figées, de l'ELF, de l'image, de l'ABI, des partitions, des 23 archives et CLI, et de la sélection effective des correctifs isolés FS/LVGL/JPEG. La preuve finale et ses dates/empreintes sont dans `VERIFICATION.md` et `tools/verification/firmware-verification-0846.json`. Les compilations préparatoires rejetées pendant les modifications ne sont pas présentées comme des builds livrables. Les scripts Windows sont fournis mais non exécutés dans cette livraison.

La compilation Arduino habituelle masque des avertissements. Une passe distincte enlève cette suppression et active Wall/Wextra ainsi que les erreurs de retour, variable non initialisée et usage après libération sur **14 unités critiques**, comparées à la référence authentifiée. La comparaison donne **368 avertissements avant et 368 après**, avec les catégories détaillées jointes ; il n'est pas prétendu que tout le projet compile sans avertissement.

Dix-huit rapports actuels couvrent les 18 fichiers de production modifiés : garde réseau, worker et UI Cartes, transport/cycle de vie audio, pilote réel et LVGL d'affichage, noyau/GUI Dessin, liste Média, série et compilations cible. Exemples de portée : 100 cycles de radio dans le garde, 200 transports + 100 producteurs + 50 pauses réelles du moteur dans le test audio, 400 cycles de tâches, 5 000 opérations Dessin comparées après chaque étape, et tests d'allocation/annulation. Les nombres d'assertions ne sont pas additionnés pour prétendre à autant d'essais indépendants.

ASan/UBSan ont été utilisés dans les suites hôte concernées. LSan est désactivé parce que l'environnement empêche son inspection des processus ; aucune réussite LSan n'est revendiquée. Les propriétaires testés utilisent aussi une comptabilité explicite des allocations, handles et réservations. L'absence de ressource restante dans ces scénarios ne garantit pas l'absence de fuite avec tous les périphériques physiques.

## Mémoire disponible sur appareil : relevés encore nécessaires

Le total physique HP inclut le cache L2 et des régions déjà occupées avant `malloc`. Les valeurs « interne », DMA, TCM/SPM et LP se recoupent selon les capacités : elles ne s'additionnent pas en nouveaux volumes disponibles. Les documents, images, listes et historiques compatibles privilégient déjà la PSRAM ; les allocations explicites des listes Média empêchent désormais un repli en SRAM en cas de pression PSRAM, sans revendiquer un gain normal lorsque le malloc générique choisissait déjà PSRAM.

Les diagnostics existants `m`/tâches donnent total libre, minimum, plus grand bloc et marges de pile. Les relever au repos, après fermeture de Cartes puis après retour du worker, et au refus radio/Meshtastic permet de distinguer manque interne, fragmentation et données volontairement conservées. Aucun buffer en cours d'usage, document non enregistré ou service de fond n'est purgé pour améliorer ces chiffres. Procédure : `TESTS_TAB5_0.8.46.md`.
