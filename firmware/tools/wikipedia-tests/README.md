# Contrôles ciblés Wikipédia 0.8.16

Exécuter depuis Windows avec Python et Visual Studio 2022 Community :

```powershell
python tools/wikipedia-tests/run.py
python tools/wikipedia-tests/test_cache.py
```

Le premier script crée des données synthétiques dans un répertoire temporaire et
compile les sources réelles `Tab5Zim.cpp`, XZ et Zstandard avec MSVC. Aucune carte
SD physique n'est modifiée. Le second utilise des services simulés et des fichiers
temporaires pour vérifier le cache Python.

Contrôles effectués avec succès :

- index de liens lu par blocs de 512 octets, reprise à un curseur, ligne trop
  longue, taille excessive et échec de lecture ;
- contenu XZ identique avec et sans coopération ; annulation pendant le décodage
  et rejet d'entrées tronquées sans résultat validé ;
- article synthétique de 3 420 046 octets dans des archives brute, XZ et Zstandard
  (blocs Zstandard bruts), avec texte et liens identiques ;
- ancien XZ annonçant un dictionnaire de 64 Mio, décodé avec le chemin compatible
  borné du lecteur ;
- annulation, refus d'allocation, erreur de lecture et sortie pleine : absence de
  fuite parmi les allocations instrumentées du lecteur ;
- cache JSON endommagé ou d'un type incorrect, nettoyage limité aux noms générés,
  annulation des illustrations et rejet d'un index d'images trop volumineux.

Sur les données synthétiques, les pics des **allocations instrumentées par le
lecteur** sont 17 576 octets (brut), 3 478 582 (XZ) et 3 705 638 (Zstandard).
Ils ne mesurent pas la mémoire totale du firmware, de MicroPython ou des petites
structures internes XZ. Ces tests ne certifient ni la fluidité de LVGL ni les
périphériques du Tab5 physique.

Les bornes des articles et des décodeurs préexistantes sont conservées. Les
lectures ZIM sont désormais découpées en blocs d'au plus 32 Kio. Le callback XZ
est propre à chaque décodeur ; il coopère tous les 8 Kio de sortie LZMA comprimée
et aux transitions du flux, y compris en mode `XZ_SINGLE`. Son refus laisse le
lecteur signaler l'annulation et libérer les ressources normalement.
