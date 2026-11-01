# Vidéo — audit et corrections 0.8.48

Référence : archive 0.8.47 restaurée et SDK ESP-IDF 5.5.4 du paquet M5Stack 3.3.9. Aucune Tab5 physique n'a été utilisée. Les résultats ci-dessous ont été réexécutés sur les sources 0.8.48 présentes, après la perte du précédent espace temporaire.

## Causes établies dans le code

- `jpeg_decoder_get_info()` réserve et initialise un `jpeg_dec_header_info_t` complet pour chaque image. La sortie « sampling factor cannot be recognized » quitte la fonction sans libérer cette allocation. Le test de 100 entêtes concernés reproduit 100 allocations retenues. Taille de cette structure sur l'hôte 64 bits : 3 200 octets ; ce chiffre ne constitue pas une mesure de mémoire sur Tab5. `MALLOC_CAP_DEFAULT` suit la politique du projet : les allocations peuvent venir de PSRAM et ne doivent pas être présentées comme un gaspillage systématique de SRAM interne.
- Cette fonction recherche SOF0 sans respecter les longueurs APP/EXIF : elle peut retenir la taille d'une miniature embarquée. La prélecture de taille n'est pas un décodeur et ne protège pas les tableaux du vrai parseur natif.
- Dans le parseur CPU JPEG natif, les indices DQT et DHT, le nombre cumulé de tables DQT, le nombre de symboles DHT et certains facteurs SOF ne sont pas bornés avant écriture/calcul. Les tests séparés reproduisent des accès hors limites pour DQT/DHT et une division par zéro pour un SOF gris de facteur nul. Il ne s'agit pas d'une affirmation d'accès hors limites dans `jpeg_get_char` : cette fonction contrôle la fin du buffer et retourne zéro.
- `jpeg_alloc_decoder_mem()` effectue un calloc des trois images PSRAM. Pour trois images RGB565 de 1280×720, cela écrit 5 529 600 octets sur le CPU avant que le DMA les remplisse.
- L'agrandissement d'entrée JPEG copiait toute l'ancienne capacité, même pour une nouvelle partie multipart ou un nouveau chunk AVI qui allait remplacer ces octets. Le flux MJPEG brut, lui, doit conserver son préfixe reçu.
- Le worker publiait `_taskRunning=false` avant `vTaskDelete`. La tâche idle FreeRTOS pouvait donc n'avoir pas encore libéré les 24 Kio de pile quand un remplacement déjà demandé créait une nouvelle tâche. Cela établit une possibilité de chevauchement de mémoire ; cela ne prouve pas que chaque manque de mémoire observé sur l'appareil vient de cette fenêtre.

## Corrections

1. `Tab5JpegHeader.h` effectue une lecture bornée, sans allocation, du SOI jusqu'au SOS. Les segments APP/COM sont sautés par longueur ; SOF0, DQT, DHT et DRI sont vérifiés avant les accès natifs dangereux. La taille et le MCU correspondent à l'image principale, y compris au padding gris. Le décodage matériel existant et ses erreurs restent utilisés. Les MJPEG sans DHT restent permis, conformément aux tables par défaut du backend. Les limites ajoutées correspondent aux capacités et tableaux du backend natif ; ce n'est pas une revendication de validation de tous les flux JPEG ni de leur entropie. Les facteurs légaux Cb/Cr de 1 à 4 restent acceptés : le SDK les transmet au matériel et dimensionne la sortie DMA selon le MCU de Y. Le banc vérifie ce calcul natif sur 16 combinaisons ; il ne qualifie pas matériellement ces formats. Le SDK peut toujours retourner une erreur de décodage.
2. Les images utilisent la même capacité PSRAM et le même alignement d'adresse/taille cache que l'allocateur natif épinglé, sans mise à zéro préalable. Aucun framebuffer n'est publié avant un décodage réussi et une longueur de sortie cohérente. Les tampons partiellement écrits lors d'une erreur restent hors de l'affichage.
3. L'entrée JPEG ne copie que le préfixe initialisé lorsque le MJPEG brut grandit. Les autres modes remplacent directement le contenu. Une allocation ratée conserve l'ancien buffer.
4. Après fermeture de l'entrée, du décodeur et des baux SD/audio, le worker consomme lui-même le dernier remplacement déjà en attente. Sa pile est réutilisée et il quitte toujours la tâche lorsqu'aucune demande n'existe : aucune tâche vidéo inactive permanente n'a été ajoutée. `stop()` réaffirme l'annulation sous mutex pour couvrir la préparation concurrente d'un remplacement. `service()` conserve le rattrapage des demandes arrivées pendant la publication finale de sortie.
5. La préparation d'une nouvelle lecture conserve un framebuffer encore référencé par LVGL. Le parcours UI normal détache l'ancienne image avant remplacement. Le backend ne réalloue pas une image attachée si les dimensions augmentent ; le parcours normal de fermeture/détachement reste nécessaire.

## Tests réellement exécutés

Commandes depuis la racine du projet :

```
python3 tools/video-tests-0848/jpeg_probe.py
python3 tools/video-tests-0848/buffers.py
python3 tools/video-tests-0848/lifecycle.py
```

- JPEG : 183 456 assertions, 16 fichiers JPEG baseline produits par Pillow, 100 000 entrées de fuzz déterministes. Parmi les 11 601 entêtes de fuzz acceptés, les entêtes hors limite de l'application ne sont pas envoyés au backend. Au total du banc, 9 651 parcours des vrais parseurs CPU SDK ont été effectués. Trois sous-processus reproduisent les défauts natifs sous ASan/UBSan. Les 100 fuites du probe natif deviennent zéro allocation pour le nouveau probe.
- Buffers : 200 cycles de décodage avec les méthodes de production extraites, heap et DMA contrôlés. Poison ASan sur les octets non initialisés, échec partiel de décodage, longueur de sortie incohérente, annulation, frame LVGL épinglée, OOM de chaque image et du décodeur, agrandissement d'entrée raté. Zéro buffer et zéro handle simulé retenus en fin de cycle. Le calloc natif exact est exécuté sur la frontière heap instrumentée pour mesurer les 5 529 600 octets supprimés.
- Cycle de vie : 653 assertions, 208 passages du corps worker contrôlé dans 5 tâches créées ; 200 remplacements successifs réutilisent la même tâche. Sont vérifiés : dernier choix conservé, annulation avant/après préparation, retrait SD, détachement, frame épinglée, erreur d'admission, fin de tâche. Le corps d'I/O de `taskMain` est simulé ; ses méthodes de cycle et sa fin de nettoyage sont celles de production. Ce banc ne teste pas une vraie connexion TLS, un serveur ni LVGL.
- Compilation cible de `Tab5Video.cpp` et ses deux entêtes effectuée avec la base de commandes restaurée Arduino/P4 : réussite, journal sans warning. La compilation complète est rapportée séparément par la validation de livraison.

LeakSanitizer n'est pas utilisable dans cet environnement ; ASan/UBSan sont actifs et les allocations concernées sont suivies explicitement. Les rapports JSON, empreintes des sources et journaux sont dans `tools/verification/video-*-0848*`.

## Limites et essais matériel nécessaires

- L'absence totale de flash, de reset ou de manque de mémoire sur un second flux reste à vérifier sur Tab5. Ni le débit PSRAM, ni le temps de handshake TLS, ni la fragmentation réelle du tas ne sont mesurés ici.
- Une demande faite après la publication finale d'arrêt peut encore nécessiter une nouvelle tâche avant le passage de la tâche idle. La réutilisation corrige la fenêtre des remplacements déjà en file ; elle n'est pas une garantie universelle d'absence de chevauchement de piles.
- Essai proposé : démarrer puis remplacer 20 fois deux flux MJPEG de dimensions différentes depuis l'UI, dont un TLS, fermer pendant l'ouverture, puis répéter après Cartes. Conserver les lignes `[VIDEO][MEM]`, les erreurs JPEG, les événements diagnostics et le rapport mémoire après fermeture. Vérifier retour à une mémoire libre/bloc maximal stable ; préciser si le manque survient avant connexion, à l'allocation des images ou après décodage.
- Enchaîner ensuite une vidéo sans audio puis une vidéo AVI PCM avec une radio en cours, puis revenir au média initial. Le test doit confirmer les règles existantes de priorité et reprise audio, sans sauvegarde forcée.
