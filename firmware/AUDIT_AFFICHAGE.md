# Affichage — référence historique 0.8.3 / 0.8.5

Ce document conserve l’audit historique du backend d’affichage. Il ne valide pas les alignements des interfaces de la version courante ; les contrôles courants et les essais physiques restants sont dans VERIFICATION.md.

L’utilisateur a confirmé la stabilité de **Tab5OS 0.8.3** sur sa tablette. Cette version sert de référence à la 0.8.5 ; les anciennes hypothèses et comparaisons de scintillement ne décrivent plus l’état de livraison.

La 0.8.5 conserve le backend d’affichage, l’alternance des deux framebuffers, RGB565, la synchronisation du cache et les protections de réutilisation. Le contrôle mémoire périodique utilise les compteurs libres ; les vérifications des blocs contigus restent réservées aux admissions et opérations explicites. Les options SDK, horloges et timings de dalle ne sont pas modifiés.

| Composant conservé | Version / configuration |
|---|---|
| Cœur Arduino M5Stack / ESP-IDF | 3.3.9 / 5.5.4, ESP32-P4 `prev3`, DIO |
| M5Unified / M5GFX | 0.2.23 / 0.2.30 |
| LVGL | 9.6.0, rendu logiciel, une unité, sans PPA |
| PSRAM / cache L2 | 200 MHz / 256 Kio, lignes de 128 octets |
| Présentation | RGB565, deux framebuffers alternés |

Les nouveautés applicatives utilisent cette pile existante. Le décodeur WebP ajouté ne remplace ni LVGL ni M5GFX et ne modifie pas leurs réglages.

La confirmation de la base 0.8.3 est un résultat utilisateur. Les tests hôtes de la 0.8.5 vérifient le comportement logiciel ; ils ne mesurent pas son affichage physique. L’essai prolongé de l’accueil, des menus et des nouvelles fonctions reste à effectuer sur le Tab5. Voir `VERIFICATION.md`.

English: v0.8.3 was confirmed stable on the user’s tablet. v0.8.5 retains its display backend, periodic memory policy and existing library/SDK settings. Host tests do not replace physical testing of this release.
