# Flashs Média et accueil : contrôle indépendant du chemin d'affichage

## Résultat

Aucune cause matérielle des flashs n'est démontrée. Les modules de présentation, cache, barrières et observation DSI de 0.8.44 sont **octet pour octet identiques au ZIP 0.8.43 livré**, dont l'empreinte est vérifiée par le script de contrôle. Aucun nouveau délai, reset, changement de fréquence de dalle, changement de priorité AXI ou suppression d'image n'est introduit dans cet ensemble.

L'interface Média 0.8.43 avait des demandes périodiques d'instantanés inutiles. Toutefois, ses fonctions de mise à jour réellement exécutées avec LVGL ne produisent déjà aucun transfert en l'absence de changement d'état. Leur suppression réduit les lectures ; elle ne prouve pas la résolution des flashs. La correction Média plafonne toujours les indications continues à environ 4 Hz, malgré les révisions moteur plus fréquentes, pour éviter une augmentation accidentelle du travail d'affichage.

## Vérification du chemin M5GFX réel

Les sources exactes M5GFX 0.2.30 restaurées pour la compilation courante ont été relues :

1. `displayFlush` appelle `startWrite` une fois au début d'une image LVGL, puis `pushImage` pour chaque zone et `endWrite` uniquement sur la dernière zone.
2. `LGFXBase::pushImage` imbrique lui-même `startWrite`/`endWrite`. `Panel::endWrite` n'appelle l'affichage automatique que lorsque son compteur imbriqué revient à zéro. Les zones intermédiaires ne déclenchent donc pas cette étape.
3. `Panel_DSI` hérite de `Panel_FrameBufferBase::display`. Cette fonction effectue les écritures cache nécessaires puis remet ses bornes modifiées à zéro. **Elle ne soumet aucun `esp_lcd_panel_draw_bitmap`**. Il n'y a pas de bascule DSI cachée par zone au sein de ce chemin.
4. `Panel_FrameBufferBase::writeImage` copie les pixels dans les lignes du framebuffer choisi. Les transactions de la classe sont synchrones ; ce chemin ne démarre pas une copie asynchrone de pixels vers le DSI.
5. Après `M5.Display.endWrite`, `tab5DisplayCacheEndFrame` appelle `tab5DisplayPresentEndFrame`. Ce dernier soumet une seule fois le framebuffer terminé, après succès de la synchronisation du cache, et conserve sa barrière de deux événements ultérieurs avant réutilisation de l'ancien buffer.
6. Le BSP demande deux framebuffers au pilote IDF. Tab5OS les réutilise ; il n'alloue pas une troisième image complète pour la présentation. Le miroir de la précédente zone modifiée n'est réalisé qu'à la demande d'une nouvelle image.

Ces constatations sont une lecture des dépendances réelles, pas un test électrique du DSI. Les empreintes des sept fichiers M5GFX examinés sont dans `tools/verification/display-media-0844.json`.

Une branche importante doit être relevée sur l'appareil : si `tab5DisplayPresentInit()` renvoie faux, l'OS conserve le fonctionnement natif M5GFX et indique **« double buffering: unavailable »** sur la liaison de diagnostic. Ce repli garde la fonction d'affichage disponible, mais ne bénéficie pas de la barrière double tampon Tab5OS. Sans ce relevé, on ne sait pas quel chemin fonctionne sur la tablette concernée. La présence d'une cause de refus ne doit pas être inventée : alignement, caractéristiques du BSP, mémoire de métadonnées et handles doivent être observés.

La documentation IDF **5.5.4**, et non la branche courante 6.x, distingue la fin de copie du tampon utilisateur de la fin de rafraîchissement du framebuffer. Elle précise aussi que la bande passante DMA et les paramètres de la dalle peuvent être impliqués dans des scintillements. C'est une piste à mesurer ici, pas un diagnostic de cette Tab5 : [documentation DSI ESP-IDF 5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/peripherals/lcd/dsi_lcd.html). La source C distante du pilote n'a pas pu être récupérée lors de ce contrôle ; aucune nouvelle affirmation sur l'ordre interne de ses interruptions n'en est déduite.

## Revalidation réellement exécutée

Le script `tools/display-media-tests-0844/run.py` relie les modules actuels `Tab5DisplayRefresh`, `Present`, `Cache` et `Activity` à LVGL 9.6 réel, avec ASan/UBSan et le modèle existant séparant cache CPU, RAM externe et buffer balayé.

Résultat : **2 759 578 contrôles réussis**, 96 publications dans les scénarios de changements visibles et d'erreurs injectées. Sur 180 secondes simulées de changements d'un libellé caché : **0 publication, 0 écriture cache, 11 250 événements de balayage**. L'unique timeout du scénario global est une panne DMA injectée volontairement puis résolue ; ce n'est pas un défaut matériel observé.

Le modèle vérifie notamment la conservation des pixels inchangés et l'absence de modification du buffer encore balayé. Il ne mesure ni débit PSRAM, ni cadences réelles des interruptions, ni alimentation, ni qualité MIPI, ni latence de la dalle. Le pilote matériel IDF et la copie matérielle M5GFX ne sont pas exécutés sur l'hôte. Les résultats ne prouvent donc pas l'absence de flash.

## Relevé court sur Tab5

1. Conserver le journal de démarrage, en particulier `[DISPLAY] double buffering: active/unavailable`, la version P4/C6 et la révision de la tablette/dalle. Si la présentation est indisponible, le signaler avant d'interpréter les autres mesures.
2. Ouvrir Média sans lecture pendant 60 s, puis lancer un fichier audio local pendant 60 s. Noter les instants des flashs et s'ils concernent toute l'image ou seulement une zone. Répéter 60 s sur l'accueil sans interaction.
3. Utiliser des relevés de diagnostic **à la demande** au début et à la fin, sans naviguer vers un autre écran pendant la fenêtre. Relever les compteurs de publications/bascules, frames/zones/pixels, dernière grande zone et service, erreurs cache/publication, timeouts et manques DSI avec leur horodatage. Ne pas imprimer continuellement : la liaison série elle-même coûte du temps.
4. Si l'observation DSI est « indisponible », le compteur zéro ne démontre pas l'absence de manque de données. Si elle est active, une hausse des underruns pendant un flash est un indice de débit/transport à investiguer. Le simple nombre d'événements de scan continu ne révèle pas un redessin LVGL.
5. Si un flash survient sans nouvelle publication et sans erreur comptabilisée, conserver quand même son horodatage : les compteurs ne couvrent pas toutes les défaillances électriques ou de dalle. Si les publications ou grandes invalidations correspondent aux flashs, utiliser le service/source relevé pour isoler le producteur.

Le script de test et ce relevé n'imposent aucune écriture de fichier utilisateur, modification de partition, formatage ou sauvegarde automatique.
