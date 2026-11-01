# Régressions réseau/média 0.8.42

Ces scripts compilent du code de production réel avec des frontières SDK simulées. Ils n’effectuent aucun essai physique. GCC hôte C++17, Python 3, ASan/UBSan requis. Exécuter depuis la racine du projet :

```sh
python tools/media-tests-0842/accessors.py
python tools/media-tests-0842/video.py
python tools/media-tests-0842/http.py
python tools/media-tests-0842/network.py
python tools/media-tests-0842/recording.py
python tools/media-tests-0842/collect.py
```

Les fixtures historiques du projet sont réutilisées sans modifier leurs rapports. Les nouveaux résultats se trouvent dans `tools/audit-0842/`. Pour reproduire le comparatif avec l’archive .41 d’origine :

```sh
python tools/media-tests-0842/accessors.py --baseline-zip /chemin/Tab5OS_0.8.41_Corrections.zip
python tools/media-tests-0842/http.py --baseline-zip /chemin/Tab5OS_0.8.41_Corrections.zip
python tools/media-tests-0842/catalogue_sort.py --baseline-zip /chemin/Tab5OS_0.8.41_Corrections.zip
```

La vidéo teste explicitement les remplacements demandés avant entrée du worker, la dernière intention, Stop, la publication après fermeture, la propriété des frames et les échecs de démarrage. L’adaptateur String compte les copies : ce n’est pas un compteur malloc de la puce. HTTP utilise les vrais fichiers POSIX temporaires et injecte des EIO ; les échanges HTTP/TLS restent simulés. Les résultats ASan/UBSan n’autorisent pas à affirmer l’absence de fuite matérielle ; LeakSanitizer est désactivé. Voir `tools/audit-0842/network-media.md` pour les essais matériels et les résiduels.

Le test recording.py exécute les vrais helpers WAV et la finalisation avec erreurs courtes, errno EIO de flush/close et getWriteError() nul, comme la limite observée dans le SDK. Un échec de sauvegarde demandé laisse le fichier partiel déjà écrit ; aucun nouvel enregistrement automatique n’est introduit.
