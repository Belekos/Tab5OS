# Tests JPEG Dessin 0.8.34

Exécuter avec Python 3, Pillow, GCC et une copie **originale** de M5GFX 0.2.30 :

```sh
python3 tools/image-codec-tests-0834/run.py --m5gfx /chemin/M5GFX --build /chemin/temporaire/jpeg
```

Le banc prépare sa copie isolée avec le même helper que le firmware. Il compile
les véritables codecs et le moteur image sous ASan/UBSan, puis vérifie les JPEG
standard et gris, des gradients/bruits/damiers, les réductions 1/2, 1/4 et 1/8,
les dimensions limites, les lectures partielles, les erreurs et l’annulation.
Un fichier valide de 16 Mio est accepté et un fichier plus grand est refusé.
Les refus d’import préservent le document, ses pixels et son historique.

L’export est décodé indépendamment avec Pillow/libjpeg pour contrôler ses
dimensions et la composition sur blanc. Les injections d’erreur d’écriture
vérifient la conservation du fichier précédent ou de sa sauvegarde récupérable.
Les deux variantes amont fautives sont compilées avec les mêmes options pour
reproduire séparément le décalage négatif et la copie avec chevauchement.

Toutes les allocations propres au codec passent par l’allocateur instrumenté du
document. LeakSanitizer est désactivé dans ce banc ; son compteur vérifie zéro
octet restant après chaque scénario. Les conteneurs de fixtures du banc ne
représentent pas le chemin de lecture progressive en production.

Ces essais hôte ne mesurent ni la vitesse de l’ESP32-P4 ni le comportement d’une
carte SD réelle. JPEG progressif, conservation des métadonnées et rotation EXIF
automatique ne sont pas pris en charge. Le document importé est limité à 1024
pixels par côté, la source à 8192 pixels par côté et 16 Mio.
