# Décodeur WebP local pour Tab5OS 0.8.4

Provenance : projet officiel WebM, libwebp 1.6.0, archive
https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.6.0.tar.gz
SHA-256 : e4ab7009bf0629fd11982d4c2aa83964cf244cffba7347ecd39019a9e38c4564
Documentation officielle : https://developers.google.com/speed/webp/docs/compiling

Les 30 fichiers C retenus sont uniquement les unités du décodeur, le démultiplexeur et leurs dépendances scalaires. Aucun encodeur C, assembleur, SIMD, thread décodeur, outil ou binaire précompilé n’est ajouté. Les en-têtes auxiliaires d’origine restent disponibles. COPYING (licence BSD), PATENTS (licence des brevets) et AUTHORS sont fournis sans modification.

Modifications locales : chemins d’inclusion relatifs ; configuration privée injectée dans chaque fichier C ; WebPSafeMalloc/Calloc/Free redirigés vers l’allocateur PSRAM plafonné ; points de coopération/annulation dans ParseFrame, DecodeImageData/DecodeAlphaData et les groupes Huffman. Les points lossless sont espacés de 256 opérations pour limiter le coût de contrôle. Les algorithmes de décodage, filtrage, mise à l’échelle et conversion ne sont pas remplacés. `tab5_config.h` et `src/webp/config.h` ne modifient aucune option globale du SDK, du compilateur ou de M5GFX.

Toutes les allocations du décodeur passent par Tab5WebPPort.cpp : PSRAM obligatoire sur ESP32, plafond cumulé de 12 Mio, aucun secours par le tas interne. Le plafond inclut en-tête d’allocation, fichier encodé (2 Mio maximum), travail libwebp, RGBA réduit et RGB565 final. Les dimensions de la source sont limitées à 4096 par axe et 4 194 304 pixels ; la sortie respecte la limite de l’appelant et 1 048 576 pixels au maximum. Une image respectant les dimensions peut néanmoins être refusée si son décodage dépasse le budget de travail.

Le décodeur natif réduit avant la sortie RGBA. Les transparences sont composées sur blanc. Une animation affiche uniquement sa première image complète à sa position dans le canevas ; aucune animation, lecture vidéo ou minuterie n’est lancée. Le démultiplexeur valide le conteneur et la position de cette image. Le port sérialise les appels pour isoler le budget et les points de coopération.

Audit P4 initial (-Os, sections séparées) : plus grande pile d’une fonction de décodage appelée = 1056 octets (VP8LBuildHuffmanTable). Les utilitaires communs d’encodage GetColorPalette et PaletteSortModifiedZeng ne sont pas appelés par le décodeur et sont éliminés par --gc-sections. La pile cumulée et l’exécution sur matériel restent à mesurer sur Tab5.
