# Ressources embarquées, construction et Flash — 0.8.42

La référence récupérable est le ZIP 0.8.41, SHA-256 `01389322d10ae80b668ce369b2586e41831af03e3e407ecd3d60da62dd2667ff`. `build-baseline.json` relève les empreintes réellement présentes dans cette archive ; il ne constitue pas une nouvelle mesure sur tablette.

## Constat confirmé et correction

Les 40 petits fichiers officiels servant à la réparation explicite des ressources étaient dupliqués en texte brut dans le firmware : 377 704 octets, alors que la version livrée ne laissait que 243 232 octets dans une partition OTA de 6 553 600 octets. Zlib était déjà embarquée pour Git. Le générateur conserve exactement tous les octets des fichiers SD et compresse chaque fichier séparément : 113 783 octets après mise à jour du manifeste Factory (378 724 octets avant compression). Le gain théorique des seules données est de 264 941 octets pour les ressources finales ; le gain net final est déterminé par le nouvel ELF/BIN, avec le code ajouté et les autres corrections.

`Tab5SystemResources.cpp` décompresse uniquement lorsqu'une réparation manuelle est demandée. Chaque invocation traite au plus 4 Kio de sortie ou d'écriture ; la relecture reste limitée à 512 octets. Un seul fichier décompressé est conservé en PSRAM, borné à 128 Kio plus un octet de détection de débordement. Zlib emploie des allocateurs explicitement PSRAM, avec un plafond de travail de 48 Kio. L'admission réserve ces futurs besoins ; les allocations sont déduites de la réservation dès leur matérialisation. La mémoire de travail est libérée après décompression et le fichier après validation/installation ou à chaque erreur/annulation. Aucune allocation de décompression ni écriture SD n'a lieu au repos ou automatiquement au démarrage.

Le décodeur exige un flux fini, son checksum zlib valide, une taille décompressée exacte et l'absence de données compressées supplémentaires. L'écriture temporaire, flush, contrôle de longueur et relecture octet par octet restent nécessaires avant renommage. Les fichiers présents, même vides ou personnalisés, ne sont jamais remplacés. Un lease de stockage partagé reste détenu jusqu'à la fermeture des handles et empêche le démontage de la carte pendant l'opération ; il ne constitue pas une exclusion mutuelle de toutes les écritures. La vérification finale de destination et le renommage FAT sans remplacement préservent le comportement antérieur.

## Validation exécutée

`system-resources-0842.json` : production réelle et zlib embarquée compilées avec ASan/UBSan, adaptation de stockage avec fautes. Reconstruction des 40 ressources, refus d'admission, échec des quatre allocations PSRAM avec reprise, flux tronqués/checksum invalide/tailles invalides, écriture courte, relecture corrompue, échec de commit, annulation, fichier concurrent et collision de répertoire. Les allocations sont comptées explicitement et doivent toutes être restituées après chaque fin/échec. Le pic hôte est de 95 627 octets ; ce n'est pas une mesure de tas sur ESP32-P4. LeakSanitizer est désactivé car l'environnement ne permet pas son inspection `/proc`; ASan/UBSan et le comptage explicite restent actifs.

`system-resources-target-0842.json` : compilation des sources actuelles avec le vrai SDK ESP32-P4. `resources-0842.json` compare également les flux reconstruits aux fichiers effectivement distribués, à leurs empreintes Factory et au générateur. Les anciennes preuves restent historiques.

## Limites et essai matériel

Sur une copie de SD, conserver un fichier officiel modifié et un fichier officiel vide, enlever un autre fichier de support, lancer Réparer les ressources, vérifier que seul le fichier absent revient. Annuler une réparation puis recommencer. Vérifier la réactivité de l'interface pendant l'opération et les ressources mémoire avant/après. Ne pas employer la carte de travail comme support de test de coupure d'alimentation. La résistance réelle de FAT/contrôleur aux coupures n'est pas démontrée par les simulations hôte.
