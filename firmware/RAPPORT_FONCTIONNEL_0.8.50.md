# Tab5OS 0.8.50 — Cartes, Réveil et Dessin

Référence : archive 0.8.49 validée par votre retour pour les flashs, l’audio et Dessin. Cette version traite les détails fonctionnels demandés. Elle conserve l’architecture d’affichage, les pilotes audio, la gestion de cache et les mécanismes mémoire stabilisés.

## Périmètre

| Demande | Évolution |
| --- | --- |
| Recherche Cartes sans clavier physique | Bouton d’ouverture du clavier virtuel, même avec un clavier USB branché ; champ focalisé et dialogue placé au-dessus du clavier. |
| Picture / Pictures | `/Picture` devient le chemin commun des nouveaux enregistrements concernés. Les fichiers déjà présents dans `/Pictures` ne sont ni déplacés ni effacés. |
| Réveil | Icône distincte de Média, quatre tonalités et préécoute indépendante de l’enregistrement des réglages. |
| Surbrillance | Couleur de sélection de l’OS appliquée aux boutons, cases et options des listes déroulantes ; focus clavier distinct. |
| Texte dans Dessin | Police de l’OS avec ses métriques, accents et antialiasing ; taille réglable. |
| Remplissage en volume | Mode Relief : effet raster 2D calculé à partir du contour et des trous, avec largeur et direction d’éclairage. |
| Déplacer une sélection | Conservation séparée du contenu déplacé et du fond recouvert, y compris lors de déplacements répétés, avec annuler/rétablir. |
| Édition de sélection | Copier, couper, coller et supprimer par commandes tactiles et raccourcis ; presse-papiers conservé entre documents ouverts dans Dessin. |
| Réglage de l’image | Libellé Couleurs et luminosité, avec indication de la portée sélection/calque. |

## Intégrité et mémoire

La table de partitions et les politiques de sauvegarde restent inchangées. Aucune fusion automatique de dossiers n’est effectuée : elle pourrait écraser des fichiers homonymes ou casser des chemins enregistrés. Les nouvelles écritures utilisent le chemin commun.

Le déplacement conserve le fond pour pouvoir le restituer ; ce comportement demande une mémoire auxiliaire que l’ancien déplacement destructif n’utilisait pas. Toutes les allocations restent soumises au budget de Dessin. Les erreurs d’allocation doivent laisser l’image dans son dernier état valide. Le contenu flottant est partagé avec l’historique et le presse-papiers appartient au document éditeur, sans être dupliqué à chaque annulation.

Le relief s’exécute avec les travaux progressifs existants et un tampon de distance borné. Il s’agit d’un effet de volume sur une image, pas d’un modèle 3D. Le texte est rasterisé dans un masque limité à la surface utile.

## Compilation et vérifications

La compilation complète ESP32-P4 a réussi, avec sources identiques avant et après la passe et 1 426 objets compilés. Chaîne conservée : Arduino CLI 1.5.1, M5Stack Arduino 3.3.9 / ESP-IDF 5.5.4, GCC RISC-V 14.2, M5Unified 0.2.23, M5GFX 0.2.30 et LVGL 9.6.0. La vérification de l’ELF confirme les liens des mécanismes natifs DSI, audio, cache, TLSF et admission mémoire de 0.8.49, ainsi que les partitions inchangées.

| Mesure issue du binaire ou de l’ELF | 0.8.49 | 0.8.50 | Évolution |
| --- | ---: | ---: | ---: |
| BIN applicatif | 6 093 264 octets | 6 103 872 octets | +10 608 |
| Capacité d’une partition OTA | 6 553 600 octets | 6 553 600 octets | inchangée |
| Marge OTA | 460 336 octets | 449 728 octets | −10 608 |
| Données statiques DATA + BSS | 93 968 octets | 93 976 octets | +8 |
| Code IRAM | 98 882 octets | 98 882 octets | inchangé |
| TCM : code / données | 794 / 64 octets | 794 / 64 octets | inchangés |
| RTC no-init | 1 200 octets | 1 200 octets | inchangé |

Les sections statiques ne représentent pas la SRAM totale ni les tas libres au démarrage. Une passe diagnostique distincte sur six unités modifiées réussit sans erreur bloquante ; elle conserve 84 avertissements, notamment appels LVGL dépréciés, mélanges d’énumérations et indentation compacte. La version n’est donc pas annoncée « sans avertissement ». Les journaux et la portée précise sont inclus dans les preuves de compilation.

| Vérification exécutée | Résultat et portée |
| --- | --- |
| Cartes / saisie / chemins | 2 204 contrôles avec LVGL, Focus et TextInput réels ; 100 cycles, clavier USB présent/absent, clavier normal/IME, Entrée/Échap, collision de noms. 12 contrôles supplémentaires des téléchargements Wikipédia. |
| Réveil et styles | 100 cycles d’ouverture/fermeture, quatre motifs PCM, préécoute/arrêt, échec de persistance, échéance pendant la terminaison du worker, couleurs résolues des états LVGL. Les nombreux contrôles d’échantillons PCM ne sont pas comptés comme des tests fonctionnels distincts. |
| Modèle Dessin | 8 238 assertions ciblées et 34 840 héritées ; 216 scénarios de refus d’allocation ciblés et 336 hérités. Le témoin compilé contre 0.8.49 reproduit l’effacement du fond et la perte d’alpha ; le code livré corrige les deux. |
| Interface Dessin | 8 677 contrôles avec le vrai éditeur, LVGL 9.6 et son allocateur natif 0.8.49 ; 41 captures. Texte OS, tailles, accents, multiligne, allocation refusée, raccourcis, presse-papiers et imports PNG/JPEG/WebP/BMP/T5I. |
| Relief | 29 386 assertions, 48 scénarios d’allocation refusée ; contour concave/troué, éclairages, progressivité, annulation et reprise des déplacements. |
| Langues et ressources | 14 packs vérifiés, formats et glyphes disponibles, ressources Factory et secours embarqué cohérents. Une seule ancienne clé d’aide remplacée par son nouveau chemin ; pas de duplication de cette longue traduction. |

Les suites natives utilisent AddressSanitizer et UndefinedBehaviorSanitizer. Les frontières matérielles sont contrôlées par les tests : leur succès ne mesure pas un écran, un haut-parleur, une carte SD ni un ordonnanceur FreeRTOS physique. LeakSanitizer n’est pas disponible ici ; les tests de Dessin vérifient séparément la restitution des allocations suivies à la destruction. Les adaptations de portabilité propres aux décodeurs hôtes sont décrites dans le manifeste UI ; elles ne modifient pas le SDK cible.

### Ressources mesurées sur hôte

| Scénario | Pic suivi | Ce que cela mesure |
| --- | ---: | --- |
| Déplacements, 1100 × 610, 20 allers-retours | 18 202 120 octets | Allocateur du modèle, avec réserve de 2 Mio pour une vue ; budget total inchangé de 18 Mio. |
| Relief, 1024 × 768 | 7 864 960 octets | Modèle et tampon du remplissage ; interface exclue. |
| Suite d’interface Dessin | 12 437 385 octets | Allocations suivies du modèle, de la vue et des codecs sur hôte ; mémoire LVGL et reste de l’OS exclus. |

Ces scénarios diffèrent : leurs pics ne sont pas à additionner ni à comparer comme des mesures avant/après sur Tab5. Les quantités de mémoire interne/PSRAM libres et les latences réelles restent à relever sur appareil.

Le fichier `tools/verification/stability-preserved-0850.json` confirme l’identité de 44 fichiers sensibles et préparateurs de compilation avec l’archive 0.8.49. L’identité des sources ne constitue pas une preuve de fonctionnement physique de l’ensemble 0.8.50. Les tests réexécutés, leurs périmètres et leurs limites sont rapportés séparément des preuves historiques des anciennes versions.

## Limites et essais sur appareil

- Le presse-papiers d’image est interne à Dessin ; il ne remplace pas le presse-papiers texte de l’OS.
- Modifier ou quitter une sélection intègre son contenu au calque ; annuler/rétablir restent disponibles selon le budget de l’historique.
- Une sélection très grande peut dépasser la mémoire disponible. Elle doit être refusée proprement, sans effacer le fond ni réduire silencieusement sa qualité.
- Le pack russe occupe 523 250 octets sur la limite de lecture existante de 524 288 octets. Il reste valide, avec seulement 1 038 octets de marge ; une prochaine extension devra contrôler cette limite.
- Les performances tactiles, l’écoute des tonalités et l’absence de régression matérielle nécessitent votre Tab5. Aucun test physique 0.8.50 n’a été effectué ici.

La procédure courte `TESTS_TAB5_0.8.50.md` couvre les usages avec et sans clavier/souris, ainsi que les enchaînements qui avaient provoqué les défauts.
