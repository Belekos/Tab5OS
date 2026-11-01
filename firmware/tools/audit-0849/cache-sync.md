# Synchronisation de cache bornée

## Défaut traité

Le SDK natif `esp_cache_msync` conserve un spinlock critique pendant le writeback ou l'invalidation de toute la plage demandée. JPEG peut synchroniser une image entière et ELF un segment entier. Sur P4, une tâche sur l'autre cœur peut également attendre ce verrou avec ses propres interruptions masquées. Le découpage déjà présent dans le rendu de l'OS ne couvrait pas ces appels internes des pilotes.

Cette propriété est vérifiée dans `libesp_mm.a` et dans la source exacte IDF 5.5.4, archivée avec sa licence et son hash dans `tools/cache-sync-tests-0849/sdk`. Aucun temps d'exécution sur P4 n'est déduit du désassemblage.

## Modification

`Tab5CacheSync.cpp` ajoute un wrapper de lien `__wrap_esp_cache_msync`. Pour les plages de données PSRAM de plus de 16 Kio et un contexte de tâche préemptible, il appelle le SDK par blocs couvrant au maximum 16 Kio de lignes de cache. Le SDK acquiert et libère son propre verrou à chaque appel; une interruption en attente peut alors être servie entre les blocs. Il n'y a ni tâche supplémentaire, ni allocation, ni délai artificiel.

Avant toute opération, l'éligibilité vérifie **la plage entière** avec les mêmes fonctions HAL de validation d'adresse et de taille de ligne que le SDK. Les débordements, flags contradictoires, alignements et bornes PSRAM sont également vérifiés. Cette prévalidation n'effectue aucune synchronisation. Tout appel non éligible est transmis au SDK avec ses arguments inchangés, y compris les appels invalides : ses codes d'erreur et ses validations demeurent effectifs.

Le chemin découpé conserve les garanties suivantes :

- C2M : tous les octets demandés sont synchronisés avant le retour; une première adresse non alignée produit un premier bloc raccourci afin de ne pas dépasser 16 Kio de lignes de cache.
- M2C : seuls les appels alignés autorisés par le SDK sont découpés. `M2C|UNALIGNED` reste un appel natif invalide.
- C2M avec INVALIDATE, plage alignée : la totalité du writeback précède la première invalidation, comme dans le SDK. Le wrapper n'alterne pas les phases à chaque bloc.
- Toute erreur d'un appel natif interrompt la séquence et est propagée; aucune réussite n'est inventée et les blocs suivants ne sont pas exécutés.
- Le wrapper et son helper résident en IRAM. Les chemins ISR, section critique, type instruction, flags inconnus et SDK différent de 5.5.4 restent natifs.

Les appels combinant C2M, INVALIDATE et une plage réellement non alignée restent également natifs. Transformer leur invalidation en M2C changerait les règles d'acceptation; arrondir la plage modifierait ses arguments. Cette exception conserve la sémantique d'origine. Les principales cibles constatées sont couvertes : JPEG C2M avec UNALIGNED sans INVALIDATE, sorties JPEG M2C alignées et partie DATA des segments ELF alignés. La synchronisation instruction ELF reste intégralement native.

## Limites et risques

Le découpage ne rend pas légitimes des écritures concurrentes dans un buffer que le pilote possède encore. Son propriétaire doit toujours attendre la fin de la synchronisation avant de lancer le DMA ou d'annoncer une image disponible. Les opérations peuvent être entrelacées avec celles de tâches utilisant d'autres buffers; chaque appel conserve le verrou natif. Le wrapper ne prétend pas fournir une transaction atomique portant sur toute une image.

La borne porte sur la **taille d'une opération**, pas sur sa durée : les accès PSRAM et la contention restent matériels. Les opérations instruction, ISR, déjà critiques et l'exception combinée non alignée ne sont pas bornées par ce mécanisme. Il ne corrige pas les parcours de heap ou les copies sous verrou d'allocateur, traités séparément.

L'activation repose sur `--wrap=esp_cache_msync`; les scripts Linux et Windows l'ajoutent. La vérification de lien doit établir que JPEG rejoint le wrapper et que wrapper/natif restent en IRAM. Une compilation Arduino externe sans ce flag conserve l'API native et n'active pas ce correctif.

## Validation exécutée

`tools/cache-sync-tests-0849/run.py --build <répertoire>` compile le fichier de production et la fonction **réelle** `esp_cache_msync` IDF 5.5.4 avec ASan/UBSan. Le HAL et les frontières FreeRTOS sont instrumentés : les octets/lignes couverts, l'ordre writeback/invalidation, les entrées/sorties de sections critiques et les erreurs sont comparés à un appel natif entier.

Résultat : **588 cas différentiels et injections de panne; 548 174 assertions réussies**. Les cas comprennent toutes les combinaisons des huit bits faibles de flags, lignes 64/128/256 octets, plages jusqu'à 32 Mio, débordement d'adresse, plage entière rejetée avant toute opération, bornes de PSRAM, ISR, section critique, appels non alignés et échec du n-ième bloc. Le code compile également avec le compilateur cible P4. Rapports : `tools/verification/cache-sync-0849.json` et `cache-sync-target-0849.json`.

Ces essais vérifient les contrats logiciels et la portée du découpage. Ils ne testent pas un cache physique, ne mesurent pas les délais d'interruption et ne prouvent pas la disparition des flashs ou des claquements. La vérification de lien et les essais de lecture JPEG/vidéo, lancement ELF et affichage sur tablette complètent cette validation.
