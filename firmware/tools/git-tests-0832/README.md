# Git 0.8.32 — commandes réelles et admission mémoire

`python3 tools/git-tests-0832/run.py --build /chemin/temporaire`
compile les 216 unités C de libgit2 livrées, les fichiers de production
`Tab5Git.cpp`, `Tab5GitPort.cpp`, `Tab5GitPull.cpp` et un client de test POSIX.
Le dossier de compilation reste extérieur à l’archive livrée. GCC, G++, OpenSSL
et Git sont nécessaires. Le test utilise ASan et UBSan. LeakSanitizer est désactivé
car le conteneur bloque l’inspection de `/proc` requise par cet outil ; les
compteurs de l’allocateur Git vérifient une allocation restante nulle après
chaque commande. Ce contrôle ne prouve pas l’absence de toute fuite hors de
l’allocateur Git.

Le profil hôte ne modifie que le fichier de configuration généré de libgit2 :
architecture 64 bits, transport HTTPS désactivé et SHA-256 fourni par OpenSSL.
Les implémentations d’index, commit, objets compressés, différences, chemins,
SHA1dc, zlib et PCRE2 sont celles de la version livrée. Les corps C du fournisseur
restent identiques. Aucun moteur Git, dépôt ou fichier n’est simulé. Les fichiers
et dépôts sont créés dans un dossier temporaire local, jamais sur un service
externe.

Le contrôle vérifie 39 scénarios de commande et 200 interrogations successives :
identité persistante, préparation d’un fichier ou d’un répertoire, suppressions,
commit effectif, refus de commit sans identité ou sans changement, différences
limitées au fichier choisi, noms contenant des crochets, consultation depuis un
sous-dossier et lecture des objets compactés par Git sur ordinateur. Git vérifie
indépendamment l’index, les messages de commit et l’intégrité (`fsck --strict`).

Le fichier `baseline_0831.cpp` est une copie byte-à-byte du moteur livré en
0.8.31 (SHA-256 `03fa2afb1b88511e020bd8bf47cecbc2265f203cc12c2742e7b8e6701d784418`).
Compilé avec la même bibliothèque, il reproduit deux défauts : `git add .` depuis
un sous-dossier préparait aussi ses voisins ; `git add -- /racine-du-dépôt`
levait une exception `std::out_of_range`. Ces comportements sont corrigés.

`python3 tools/git-tests-0832/run_memory.py` conserve la suite d’admission 0.8.31 :
46/48 Kio, réservation avant création de la pile de 40 Kio, libération des
réservations, allocations progressives et 400 cycles avec fautes injectées.
Cette seconde suite utilise le gestionnaire de ressources réel et les blocs
exacts d’allocateur et de worker, avec des capacités mémoire et un ordonnanceur
simulés. Elle ne mesure pas la mémoire physique de la tablette.

Résultats : `tools/verification/git-engine-0832.json` et
`tools/verification/git-memory-0832.json`, liés aux empreintes des sources.
Aucun clone HTTPS, échange C6, retrait de SD ou essai sur ESP32-P4 physique
n’est revendiqué par ces deux suites. L’interface et la compilation du firmware
sont vérifiées séparément.
