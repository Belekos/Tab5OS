# Dessin — interface et texte 0.8.50

Le modèle et le pipeline de vue restent distincts. Le rendu du texte utilise désormais LVGL avec la police active de l’OS : métriques, fallback, UTF-8, antialiasing, retours à la ligne et position du curseur. La police embarquée est DejaVu Sans Mono ; une autre police proportionnelle choisie dans l’OS conserve ses propres métriques. L’échelle initiale est 100 %, réglable de 25 à 400 %.

Le texte est dessiné dans un masque A8 temporaire limité à la partie utile de l’image, puis appliqué par lignes. Une seule ligne de sortie est réservée, au plus 8192 octets. Les allocations passent par le même budget de 18 Mio que le document et sa vue. Les deux réservations précèdent la restauration de l’ancien aperçu. Un refus laisse les pixels précédents et la saisie en cours disponibles ; la validation ou une commande incompatible reste bloquée jusqu’à réussite ou annulation explicite. Aucun nouveau rafraîchissement périodique n’est ajouté.

Copier, couper, coller et supprimer sont disponibles dans Édition et dans les options Déplacer. Ctrl+C/X/V et Suppr respectent les champs de saisie et les dialogues. Le collage est centré dans la vue visible et active Déplacer ; les flèches déplacent d’un pixel, Maj+flèches de dix pixels lorsque le canevas possède l’interaction. Copier seul ne recalcule pas l’image. Le presse-papiers est celui de Dessin, distinct du presse-papiers texte de l’OS.

« Couleurs et luminosité » indique explicitement la sélection actuelle ou le calque entier. Les commandes de format et de transformation restent séparées. Le remplissage Relief expose sa largeur et les quatre directions d’éclairage. Ces commandes sont traduisibles et utilisent le style partagé.

## Vérifications exécutées

`tools/image-native-tests-0850/run.py` compile l’éditeur, le modèle, les vrais codecs, le gestionnaire de focus, la saisie et LVGL 9.6 sous ASan/UBSan. L’allocateur privé TLSF et son adaptateur 0.8.49 sont exécutés sans modification de leur logique. Seuls les appels de réservation/libération matériel de l’adaptateur sont renommés à la compilation hôte pour suivre séparément les allocations LVGL et celles du document.

Le résultat courant est dans `tools/verification/image-native-0850.json`, avec les empreintes des sources et des captures. Les scénarios couvrent notamment : texte OS, changement vers une police proportionnelle, accents, multiligne, tailles 25/100/400 %, bords d’image, refus mémoire préservant le dernier aperçu, saisie et confirmation, annuler/rétablir, commandes de sélection par événements et raccourcis, collage après Nouveau et après de véritables chargements PNG/JPEG/WebP/BMP/T5I. Les refus mémoire à l’ouverture conservent document et presse-papiers. Les contrôles antérieurs de dessin, écriture atomique, libération et absence de rafraîchissement au repos restent exécutés.

Le pic `peakPsramBytes` du manifeste mesure uniquement les allocations hôtes représentant les buffers document/vue/codecs. Il ne représente ni une mesure physique de PSRAM ni la consommation complète de LVGL/OS. Après fermeture, ces buffers sont tous restitués ; après `lv_deinit`, toutes les réservations suivies de l’allocateur LVGL sont également rendues. LeakSanitizer n’est pas disponible ; cette restitution est vérifiée par les registres d’allocations des tests. Les échecs internes à la rasterisation LVGL ne sont pas tous couverts par l’injection des refus d’allocation A8.

Reproduction : préparer l’archive LVGL native avec `tools/lvgl-memory-tests-0849/run.py`, puis lancer `tools/image-native-tests-0850/run.py --lvgl <sources LVGL 9.6> --lvgl-build <archive native et lv_conf.h> --build <dossier temporaire> --m5gfx <M5GFX 0.2.30>`.

Les captures hôtes `39-os-font-text.png`, `40-selection-actions.png` et `41-colour-scope.png` ont été inspectées. Elles ne constituent pas un essai de latence, de tactile ou d’affichage sur Tab5.
