# Vérification des imports et exports Dessin 0.8.35

Le banc compile les codecs de production, le cœur RGBA, PNG, WebP et JPEG sous AddressSanitizer et UndefinedBehaviorSanitizer. Le fichier `run.py` génère les images avec Pillow, ajoute un PNG Adam7 vérifié indépendamment, puis vérifie les pixels, les dimensions natives et la libération de toutes les allocations.

Cas couverts : JPEG 1920 × 1080 et 8192 × 16 sans réduction; PNG RGBA, palette, monochrome, gris 16 bits et Adam7; WebP avec/sans perte, alpha et première image d’animation; BMP 1600 × 1200, 8192 × 16 et export à largeur impaire; T5I avec calques. Les erreurs de lecture/écriture, annulations, allocations refusées et documents hors budget doivent préserver le document précédent et son historique. Un essai reproduit le refus de pinceau sur une image Full HD quand l’aperçu et le cliché d’annulation dépassent l’enveloppe existante de 18 Mio.

Les dépendances JPEG utilisent les cinq corrections bornées de M5GFX 0.2.30. Pour miniz seulement, une copie de test désactive les accès non alignés spécifiques x86 afin d’employer le même chemin portable que le firmware RISC-V; les sanitizers restent actifs et les empreintes de cette configuration sont dans le rapport. Les dépendances du firmware ne sont pas modifiées.

```sh
python3 tools/image-codec-tests-0835/run.py --m5gfx /chemin/M5GFX-0.2.30 --build /tmp/tab5-codec-0835
```

Limites : pas de mesure sur tablette réelle; JPEG progressif refusé; pas de réorientation EXIF ni de conservation des métadonnées. Les importations dépassant la mémoire disponible sont refusées sans réduction implicite.
