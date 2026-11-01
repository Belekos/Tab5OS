# Tab5OS 0.8.16 — fiabilité et applications

Note historique de la 0.8.16. Pour l’interface et la validation courantes, lire NOUVEAUTES_v0.8.17.md et VERIFICATION.md.
- **Distribution allégée** : retrait de Mail, du navigateur Internet, de Quake et de Duke Nukem 3D. Les services réseau utiles aux autres outils sont conservés. Les données personnelles déjà sur la SD ne sont pas effacées.
- **Wikipédia** : lectures ZIM découpées et annulables, décodage XZ coopératif, index de liens tamponné avec reprise de curseur, admission mémoire avant les grosses allocations et cache corrompu géré sans bloquer la lecture. Les limites exactes et les erreurs attendues sont documentées dans WIKIPEDIA.md.
- **Applications** : catalogue local commun aux outils intégrés et aux applications installées, avec un accueil par défaut ou personnalisé. Choix des applications, ordre et icônes sont décrits dans APPLICATIONS.md. Le langage d'une application ne doit pas imposer un parcours distinct.
- **Ressources système** : réparation conservatrice et récupération des petites ressources indispensables depuis le firmware, selon le périmètre de REPARATION_SD.md. Les archives Wikipédia, documents et autres contenus volumineux ne sont pas embarqués dans le firmware.
- **Police de secours** : interface temporairement en anglais si la police nécessaire est absente ou illisible. La langue préférée reste enregistrée et revient au démarrage lorsque la police est de nouveau disponible. Les documents de l'utilisateur ne sont pas traduits ni modifiés.

## Limites

Le catalogue est local : ce n'est pas un magasin en ligne ni un compilateur C embarqué. Les applications ELF partagent la mémoire du système ; leur séparation en fichiers ne fournit pas l'isolation d'un processus de bureau.

Le formatage des supports et la récupération de fichiers effacés ne sont pas ajoutés dans cette version. Réparer les ressources du système ne répare pas la structure FAT et ne récupère pas des documents supprimés. Aucune opération de formatage n'est exécutée automatiquement.

Le coffre appareil et le catalogue MicroPython de la 0.8.15 sont conservés. Un ancien coffre nécessite toujours son ancien mot de passe ou une sauvegarde de clés utilisable. Les projets MicroPython pour d'autres cartes n'impliquent pas un téléverseur universel.

Voir VERIFICATION.md pour l'état réel de compilation et des tests. Les résultats de la 0.8.15 ne certifient pas la 0.8.16, et une compilation réussie ne remplace pas les essais matériels de Wikipédia, de la SD/USB et des ouvertures/fermetures répétées.
