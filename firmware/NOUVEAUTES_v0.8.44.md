# Corrections 0.8.44

Cette version reprend la 0.8.43 et cible les interactions médias, cartes, réseau et microSD.

- Vidéo réseau : correction d'un accès à un transport libéré, reproduit avec les méthodes du SDK exact sous ASan. Diagnostic RTC HTTP/TLS/streaming ; URL distante complète retirée des traces.
- Vidéo/audio : suspension de la musique avant toute vidéo, fermeture des ressources avant reprise. Enregistrement conservé avec vidéo muette. Bail depuis l'audio totalement inactif sans création inutile d'une pile de 16 Kio.
- Média : actualisation sur changements, échecs de copies/événements conservés et notifications du plein écran préservées. Progressions ordinaires regroupées avec la limite existante d'environ 4 Hz ; actions forcées et événements vidéo immédiats.
- Cartes : annulation HTTP coopérative et fin sans l'attente de 60/220 ms après fermeture ou priorité média. Vérification de la publication des tuiles jusqu'à la synchronisation/fermeture, rollback et causes d'erreur conservées.
- SD : ressources saturées, descripteur invalide et exclusion temporaire ne sont plus artificiellement traités comme panne de communication.
- Wi-Fi des cartes : état SDK mis en cache, indépendant d'un ancien instantané audio ; contrôle audio sans copier six chaînes.
- Flashs : commande série `d` enrichie avec les compteurs Present/DSI/LVGL, sans timer ni redessin ajouté. Le pilote n'est pas modifié sans preuve.

Compilation complète et tests ciblés réussis. Image +3 376 octets ; données internes statiques +16 octets ; partitions et 14 packs de langues inchangés. Aucun essai matériel, gain de tas réel ou disparition des flashs n'est revendiqué.

[Diagnostic](AUDIT_0.8.44.md) · [Ressources](BILAN_RESSOURCES_0.8.44.md) · [Validation](VERIFICATION_0.8.44.md) · [Tests Tab5](TESTS_TAB5_0.8.44.md).
