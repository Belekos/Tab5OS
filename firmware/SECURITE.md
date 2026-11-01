# Coffre et sécurité — 0.8.28

## Ouvrir et verrouiller

Le **coffre appareil** s'ouvre nativement, sans Python ni dérivation lente. À sa première utilisation, créer le PIN obligatoire, puis les clés de fichiers souhaitées. Une clé nommée Personnel n'est plus recréée automatiquement : toute clé, y compris la première, peut être supprimée après confirmation.

L’accès exige un **PIN de 4 à 12 chiffres** dès la première utilisation. Une mise à jour d’un coffre appareil sans PIN demande aussi d’en créer un, sans remplacer son maître ni ses entrées. Modifier le PIN exige de saisir le PIN actuel ; un champ vide ne retire plus la protection. Le PIN est demandé à chaque réouverture du coffre verrouillé. Après trois échecs, un délai croissant ralentit les essais pendant la session. Fermer le coffre ou éteindre l'écran efface la session et le cache des mots de passe en RAM ; les nouvelles lectures par shell, Python, Git et applications natives sont refusées. Une application ayant déjà reçu une valeur peut en conserver sa propre copie.

Le maître aléatoire demeure dans la mémoire interne NVS, comme dans les versions précédentes. **Le chiffrement protège la carte SD retirée. Le PIN est un contrôle d'accès de l'interface et des API normales, pas une protection contre l'extraction de la flash interne ou un firmware modifié.** Aucune programmation d'eFuse, de Secure Boot ou de chiffrement de flash n'est réalisée. Les programmes natifs/ELF partagent la mémoire et sont du code de confiance.

## Clés et fichiers

Le chiffrement crée une copie `.t5v` et conserve l'original. Le déchiffrement crée une copie ordinaire. Les originaux, les copies déchiffrées et les documents ouverts ne sont pas rendus confidentiels par la fermeture du coffre. Limite : 128 Mio par fichier ; noms et tailles des fichiers restent visibles.

La suppression d'une clé retire cette clé du trousseau courant. Elle ne supprime pas les fichiers chiffrés ni les sauvegardes de cette clé. Sans autre copie utilisable, les fichiers liés à la clé deviennent irrécupérables. La suppression de toutes les clés de fichiers ne détruit pas les entrées du coffre de mots de passe : elles utilisent une clé dérivée séparée du maître appareil.

Deux sauvegardes sont proposées :

| Type | Usage | Restauration |
|---|---|---|
| Appareil | Sauvegarde rapide, sans mot de passe de fichier | Sur le même maître appareil, après ouverture du coffre |
| Portable | Export AES-GCM protégé par un mot de passe d'au moins 12 caractères | Sur un autre Tab5 avec le mot de passe, même après perte de l'appareil d'origine |

Une sauvegarde appareil est liée à ce Tab5 ; elle ne remplace pas un export portable pour survivre à la perte, au remplacement ou à l'effacement de sa mémoire interne. Les exports portables et anciennes sauvegardes restent compatibles : PBKDF2-HMAC-SHA-256, 600 000 itérations, sel aléatoire. Cette dérivation reste coûteuse, mais s'exécute dans une tâche de fond avec contrôle d'annulation pendant le calcul. La sauvegarde appareil ne l'effectue pas. Le bouton Annuler laisse l'original intact et retire le fichier temporaire.

## Ancien coffre par mot de passe

Les anciens trousseaux conservent leur format et leur mot de passe d'origine. Une mise à jour ne peut pas retrouver un mot de passe oublié. Dans Accès du coffre, ouvrir l'ancien coffre, puis copier ses clés vers le coffre appareil. Si l'appareil possède déjà un PIN, le saisir pour autoriser cette copie. La fusion refuse les collisions d'identifiants ou le dépassement de huit clés et conserve l'ancien trousseau. Les modifications ultérieures du coffre appareil ne modifient pas cet ancien trousseau.

## Mots de passe, jetons et environnement

Le coffre conserve jusqu'à **64 entrées**, avec un nom ASCII de variable de 48 caractères au maximum et une valeur UTF-8 de 512 octets au maximum. Les noms suivent `[A-Za-z_][A-Za-z0-9_]*`. Les champs de saisie de valeurs ne passent pas par l'historique ou le presse-papiers. Les références de variables et de configuration Git contiennent des noms ; les valeurs restent dans le coffre chiffré. Voir [ENVIRONNEMENT_ET_COFFRE.md](ENVIRONNEMENT_ET_COFFRE.md) pour les interfaces shell, MicroPython et ELF.

Le stockage utilise deux fichiers chiffrés alternés sous `/System/Secrets/`. Une nouvelle version est écrite dans un temporaire, fermée puis entièrement vérifiée avant son adoption. La précédente version authentifiée est conservée pour récupérer une interruption d'écriture. Les noms et valeurs du coffre ne sont jamais écrits en clair sur la SD. Les lectures ne réécrivent pas les fichiers ; enregistrer une valeur identique ne crée pas de nouvelle version.

Ces entrées sont **liées au maître appareil**. Copier les deux fichiers SD permet une restauration sur le même appareil intact ; les exports portables de clés de fichiers n'exportent pas ce maître ni les mots de passe. Une perte du maître rend les entrées illisibles. Les anciens états restent chiffrés dans la banque de secours ; la gestion d'usure SD/NVS ne permet pas de garantir un effacement physique. Le format ne revendique pas de protection contre la réintroduction d'un ancien état chiffré valide.

## Vérification technique

Les primitives AES-256-GCM et HMAC-SHA-256 proviennent de mbedTLS. Chaque bloc de fichier authentifie sa position et l'en-tête ; le déchiffrement vérifie tous les blocs avant de publier la copie déchiffrée. Le maître appareil enveloppe son trousseau NVS. Des domaines HMAC distincts séparent les sauvegardes locales et les mots de passe. L'entropie provient du CTR-DRBG initialisé à partir du P4 ; une panne de génération bloque l'opération, sans valeur de secours prévisible.

## Connexions réseau — protections conservées depuis la 0.8.13

Les connexions Internet TLS utilisent désormais un même client avec le bundle de certificats ESP-IDF. Le contrôle du certificat et du nom d’hôte n’est plus désactivé dans les cartes, les archives Wikipédia, les flux multimédias HTTPS, les services Python et l’IRC TLS. La date doit être initialisée ; le client refuse une horloge antérieure à novembre 2023. Ce seuil détecte un appareil non synchronisé, sans garantir à lui seul l’exactitude de son horloge : la synchronisation reste nécessaire pour les dates de validité des certificats.

Mail et le navigateur Internet sont retirés depuis la distribution 0.8.16. Les applications conservées peuvent encore utiliser une URL HTTP explicitement configurée ; cette connexion reste en clair. L’imprimante locale conserve son parcours séparé de découverte puis validation/mémorisation de certificat. La désactivation temporaire de vérification dans cette seule découverte ne doit pas être assimilée à un client Internet sans contrôle.

Le magasin de certificats est celui du socle 3.3.9/IDF 5.5.4, pas une mise à jour permanente des autorités. Un serveur avec certificat auto-signé, une chaîne inconnue ou une date invalide peut donc être refusé. Les tests sur ordinateur vérifient le routage vers le transport sécurisé, les limites et l’absence de repli ; la négociation réelle avec vos serveurs doit être confirmée sur Tab5. Aucune certification de sécurité ni audit indépendant n’est revendiqué.

## Coffre inaccessible

La page verrouillée propose **Coffre inaccessible ?**, avec avertissement et saisie de `RESET`. La réinitialisation supprime le maître et les clés de ce Tab5 ; les identifiants chiffrés sont archivés sur SD mais restent inaccessibles. Les fichiers chiffrés et l’ancien coffre à mot de passe sont conservés. Les sauvegardes portables des clés restent nécessaires pour déchiffrer les fichiers correspondants. Voir [COFFRE_0826.md](COFFRE_0826.md) pour les conséquences et la reprise après interruption.

## Chargement après déverrouillage — 0.8.28

La validation du PIN et le chargement des identifiants ont des états distincts. Une erreur de lecture, de mémoire ou de déchiffrement reste visible ; l’affichage ne relance pas silencieusement la lecture des banques. Remettre la carte SD puis choisir **Réessayer** permet une nouvelle tentative explicite en arrière-plan. Ce traitement conserve les fichiers et les clés existants.

Le diagnostic du coffre garde uniquement l’étape technique en mémoire RTC pour aider à comprendre un éventuel redémarrage. Il n’y enregistre ni PIN, ni identifiant, ni valeur secrète, et n’écrit pas périodiquement sur SD. Voir [COFFRE_0827.md](COFFRE_0827.md) pour la contre-épreuve logicielle et ses limites matérielles.
