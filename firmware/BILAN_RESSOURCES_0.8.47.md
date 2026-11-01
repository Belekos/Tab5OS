# Ressources et validation — Tab5OS 0.8.47

Référence : livraison 0.8.46 authentifiée, SHA-256 ZIP `22d99628cc3ab6a9673be1886755ec9a3092aaede3f017a7e0e20443cc0816a6`. Les mesures de liaison proviennent du compilateur ESP32-P4 réel. Les mesures des scénarios hôte décrivent leurs sources et frontières contrôlées, jamais une latence physique de la Tab5. Aucun chiffre de heap libre au démarrage, CPU, FPS, autonomie, fragmentation réelle ou bruit acoustique n'est extrapolé.

## Image, partitions et mémoire liée

| Mesure, en octets | 0.8.46 | 0.8.47 | Évolution |
|---|---:|---:|---:|
| Image applicative OTA | 6 071 360 | 6 080 080 | +8 720 |
| Partition OTA | 6 553 600 | 6 553 600 | inchangée |
| Marge dans la partition | 482 240 | 473 520 | −8 720 |
| Données + BSS internes | 93 372 | 93 644 | +272 |
| `.dram0.data` | 31 428 | 31 428 | inchangée |
| `.dram0.bss` | 44 052 | 44 276 | +224 |
| `.dram1.bss` | 17 892 | 17 940 | +48 |
| `.iram0.text` | 98 610 | 98 660 | +50 |
| TCM code / données | 794 / 64 | 794 / 64 | inchangés |
| RTC noinit | 1 200 | 1 200 | inchangée |

L'image occupe **92,77 %** de sa partition ; il reste **462,42 Kio**. Cette marge impose de continuer à contrôler chaque évolution ; aucun repartitionnement ne masque sa diminution. Le binaire final a pour SHA-256 `d0818cd490ad71e6a7c6d2af29b86ab2bedd37a519cfb38b82e2e1d9bd7031bd`. Le détail des sections et les artefacts ELF/MAP sont joints aux preuves de compilation.

« Application / partition OTA » compare le programme stocké en Flash à la capacité de son emplacement applicatif. Cela ne décrit ni sa consommation RAM, ni toute la Flash. Les partitions, offsets, clés et données utilisateur ne sont pas modifiés.

Les tailles de sections liées ne sont pas des mesures de heap disponible. Sur P4, la mémoire physique HP de 768 Kio comprend des usages distincts, dont le cache L2 configuré ici à 256 Kio. Les piles et allocations internes, le code IRAM, les sections DRAM, la TCM/SPM, la LP SRAM et la PSRAM ne constituent pas un même réservoir interchangeable. Les capacités DMA recoupent des régions déjà comptées ; elles ne s'ajoutent pas à la capacité totale. Le SDK livré configure la PSRAM à 200 MHz et le P4 à 360 MHz.

## Comparaisons exécutées

| Scénario contrôlé | 0.8.46 | 0.8.47 | Interprétation |
|---|---:|---:|---|
| 100 publications Média sans changement | 27 000 000 pixels rendus | 0 | LVGL réel ; pas une mesure de FPS |
| Rendu LVGL demandé au milieu de la préparation d'une liste Média | 247 044 pixels rendus | 0 | Le contenu visible n'est plus vidé pendant la préparation ; ce scénario ne teste pas l'application Dessin |
| 40 changements réels du catalogue | 9 882 096 pixels rendus | 4 138 774 | Les lignes effectivement modifiées restent invalidées |
| Tri de 800 éléments | 801 allocations | 8 | Noms dans une arène ; 800 lectures de métadonnées dans les deux cas |
| Références réservées, même liste de 800 éléments | 8 192 octets | 16 384 octets | Surcoût PSRAM du modèle en préparation |
| Pic suivi dans ce scénario de liste | 50 592 octets | 64 768 octets | Mesure d'allocations hôte instrumentées, pas un pic global du firmware |
| Objet MediaList compilé P4 | 908 octets | 1 056 octets | +148 octets dans l'objet propriétaire |
| Objet gestionnaire d'admission compilé P4 | 820 octets | 960 octets | +140 octets statiques internes ; mutex/événements natifs |
| Réveils par tick en attendant une collecte, scénario fixé | 25 | 0 | Une attente d'événement remplace le sondage ; pas une mesure CPU |
| Écritures NVS tentées depuis l'UI, scénario de persistance | 808 | 0 | Travail exécuté par un worker temporaire ; les pauses globales Flash restent possibles |
| Activation audio avant horloges dans 24 configurations | 24 | 0 | Vérification de séquence des fonctions réelles ; aucun essai acoustique |

Les tailles d'objets ci-dessus expliquent leurs propres coûts ; elles ne doivent pas être ajoutées une seconde fois aux sections liées ou assimilées au heap global.

La nouvelle persistance utilise au maximum un job simultané : instantané en PSRAM, pile interne de 6 144 octets, priorité 1, sans affinité imposée. Aucun worker de persistance ne reste actif au repos. Les clés et le record navigateur NVS de 3 444 octets sont inchangés ; l'empreinte du record test est identique. Les scénarios après correction comprennent davantage d'erreurs injectées : le total de leurs écritures ne peut pas servir à annoncer une réduction de l'usure Flash.

Les checkpoints syntaxiques de deux index d'Écriture représentent au plus 45 232 octets sur la cible lorsqu'ils sont présents. Ils sont récupérables sous pression uniquement pour une vue cachée ; il ne s'agit ni d'une économie SRAM ni d'une libération permanente au démarrage. Texte, historique, sélection et enregistrements restent conservés. Le cache structurel utilisant encore un allocateur C++ non récupérable en cas d'OOM n'est pas évincé.

Le transfert d'affichage spécialisé n'ajoute ni framebuffer ni tâche. Sa tuile de travail est de 512 octets sur la pile ; la fonction de copie compilée P4 a une trame de 576 octets. La réduction des plages de synchronisation cache appartenait déjà à la 0.8.46 : elle n'est pas recomptée comme un gain nouveau.

Le démarrage audio utilise 256 octets de silence temporaires. La configuration livrée conserve 4 096 octets de buffers DMA I²S (256 × 8, mono actif, 16 bits), un intermédiaire PCM de 12 288 octets en PSRAM et les tailles de piles existantes. La fréquence de sortie fixe et le rééchantillonnage existaient déjà. La nouvelle séquence ne les présente pas comme des corrections nouvelles.

## Compilation et suites exécutées

Compilation complète Linux/GCC réussie le 26 octobre 2026, de 18:20:09 à 18:25:14 UTC, avec sources inchangées : **2 028 fichiers sources contrôlés, 1 916 copies Arduino vérifiées, 1 423 objets**. Arduino M5Stack 3.3.9 / ESP-IDF 5.5.4, GCC RISC-V 14.2, 23 archives de dépendances et Arduino CLI authentifiés. Image ESP32-P4, ABI, checksum, SHA embarqué, liaison des copies corrigées et partitionnement vérifiés. Les callbacks DSI/DW-GDMA restent liés en IRAM, leurs champs concernés en DRAM. Les scripts Windows sont fournis mais n'ont pas été exécutés.

Une compilation préparatoire réussie pendant les changements a été rejetée par le contrôle des sources. Une compilation ultérieure a détecté deux déclarations Arduino incohérentes après l'évolution des fonctions Média ; elles ont été corrigées, puis les tests et la compilation complète relancés. Seul le build final figé est livré. `VERIFICATION.md` et `tools/audit-0847/build-release.md` décrivent cette distinction.

Une passe distincte retire la suppression `-w` sur **11 unités** : zéro erreur avec les contrôles de retour invalide, variable non initialisée et usage après libération, mais **390 avertissements** au total. Sur les six unités comparables à la preuve authentifiée 0.8.46, le compte passe de **351 à 341** : cinq fonctions inutilisées, quatre appels LVGL dépréciés et une indentation ont disparu des diagnostics. La passe historique n'est pas réexécutée et les diagnostics supplémentaires ne sont pas comparés à un baseline inventé. Parmi les cinq unités ajoutées à cette passe, MediaSession, ResourceManager et WriterSyntaxIndex ont zéro avertissement ; Writing en a 43 et SDR six. Il n'est donc pas affirmé que tout le projet compile sans avertissement. La compilation séparée de M5Unified d'origine/corrigé conserve un même avertissement d'en-tête SDK, sans ajout.

Les suites utilisent les fichiers de production ou des fonctions extraites dont les empreintes sont vérifiées. Leurs interfaces matérielles contrôlées sont décrites dans chaque rapport ; un périphérique simulé n'est jamais présenté comme une Tab5 testée.

- Affichage : rotations, stride, ordre des octets et repli comparés à M5GFX ; chemin LVGL réel ; pilote/presentateur réel ; tests indépendants de 8 000 rectangles et 32 000 entrées invalides sans mutation. Aucun accès détecté à l'image en cours de balayage dans les scénarios testés.
- Média : modèle conservé, publications différentielles, échecs d'allocation, sélection, onglets, révisions et mutations concurrentes ; 220 vérifications d'intégration UI. Méthodes réelles audio : 921 contrôles des commandes en file, rechargements et reprises par identité stable.
- Audio : 4 504 vérifications de démarrage, 41 pannes injectées, 24 configurations ; 1 000 cycles d'attente/reprise du codec externe ; 100 cycles Audio/Vidéo ; panne du décodeur et 2 000 cycles SDR. La contre-pression native de 500 ticks à 4 kHz reste acceptée.
- Régression transport/réseau héritée de 0.8.46 : 200 transports HTTP/HTTPS, 100 producteurs, 50 pauses/reprises et 800 transactions concurrentes sur 100 cycles radio. Les deux versions passent ; aucune allocation ni client suivi restant. Cartes reste admis pendant une radio établie. Ces résultats sont des acquis conservés, pas de nouveaux gains ni un essai Wi-Fi physique.
- Persistance : 400 cycles, 4 000 services pendant une écriture bloquée, 50 000 changements concurrents de volume, erreurs partielles et reprises ; aucune allocation de job ni réservation suivie restante, au plus un job simultané.
- Ressources/Écriture : régressions d'admission sur 400 cycles, échecs mémoire et 100 cycles de restitution/reprise syntaxique. Deux relectures indépendantes ont testé la réutilisation d'un slot avec un ancien événement retardé, sans admission anticipée.

ASan/UBSan sont actifs dans les suites concernées. LSan est désactivé car l'environnement empêche son inspection des processus ; aucune réussite LSan n'est revendiquée. Les comptabilités explicites d'allocations et de réservations valident les propriétaires exercés, sans certifier l'absence de fuite pour toutes les combinaisons physiques.

L'inventaire final couvre 1 379 fichiers sources et 638 occurrences lexicales d'API : attentes, synchronisation, mémoire et création de tâches. Ce total inclut 42 occurrences de création de tâches ; il ne mesure ni 42 tâches simultanées, ni 638 attentes problématiques. Les appels `delay` sont 42 avant/après ; `vTaskDelay` passe de 89 à 88, les prises de sémaphore de 77 à 78 et les attentes EventGroup de 1 à 2. Les attentes de périphériques, temporisations de protocole et protections de propriété ne sont pas supprimées indistinctement.

## Nettoyage et conservation

Les cinq callbacks Média sans appelant ont été retirés. Le jeu de ressources Factory 0.8.46 n'est plus dupliqué dans la nouvelle distribution : 182 fichiers, 75 681 460 octets retirés de cette copie de travail uniquement. Le manifeste historique et les empreintes de migration sont conservés, ainsi que la livraison originale. Ce nettoyage de la copie de travail n'est pas un gain de taille d'archive — l'ancien paquet était déjà exclu par le filtre d'archivage — ni une économie de Flash applicative ou une suppression sur la carte personnelle de l'utilisateur.

Le contrôle de préservation compare les applications, ressources conservées et preuves historiques à la livraison authentifiée. Aucun changement destructif de la table des partitions, aucune remise à zéro des secrets et aucune sauvegarde automatique forcée de documents ne font partie de cette version.

## Mesures matérielles encore nécessaires

Les valeurs `m` doivent être relevées à états comparables : démarrage, Média, Cartes puis retour, radio en pause et après fermeture effective des workers. Comparer mémoire libre actuelle, minimum historique, plus grand bloc et marges de piles. Un minimum historique ne remonte pas après libération ; son maintien bas ne prouve pas une fuite.

Les compteurs `d` séparent transfert natif, cadence DSI, underruns et jobs de préférences. L'indicateur `observed` doit être vrai pour conclure sur un compteur DSI nul. Le profilage `p` reste ponctuel. Les parcours et critères bloquants sont dans `TESTS_TAB5_0.8.47.md` ; les défauts résiduels sont classés dans `AUDIT_0.8.47.md`. Cette livraison ne constitue pas une qualification de production sur appareil.
