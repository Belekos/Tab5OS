# Admission mémoire bornée — 0.8.49

```sh
python tools/memory-admission-tests-0849/run.py --db /chemin/compile_commands.json
```

Le banc compile les sources actuelles du gestionnaire. Il construit un vrai
heap TLSF avec les sources Espressif épinglées dans `audit-0849/heap-walk`.
Il vérifie 5 000 demandes sur un heap fragmenté, avec différentes tailles,
frontières de classes, surcharges et contraintes d'alignement. Pour chaque
demande, le résultat de la recherche non mutante est comparé au véritable
`tlsf_malloc` ou `tlsf_memalign_offs`. Un `memcmp` de toute l'arène vérifie
que l'interrogation n'en a modifié aucun octet.

Le scénario d'intégration remplit un heap puis libère un bloc sur deux.
Le total libre suffit à la demande mais aucun bloc contigu ne convient.
Le véritable `Manager` refuse cette demande pour fragmentation, collecte
un cache enregistré, puis admet la même opération. Une allocation native
vérifie le résultat ; aucun échantillon de heap complet n'est appelé.

Avec `--db`, le script utilise la base de compilation réelle du P4 pour
compiler les trois unités modifiées. Il vérifie les empreintes du SDK avant
d'activer la recherche native et contrôle le désassemblage des deux modes :
mode épinglé avec verrou natif ; mode fallback sans accès à la disposition
privée. Un SDK absent est également vérifié comme non reconnu.

Le code de production ne compile pas un autre allocateur. Il utilise les
fonctions de recherche TLSF du SDK et ses verrous. Le shim privé est limité
à ESP32-P4, IDF 5.5.4, LIGHT poisoning, sans task tracking ni allocateur ROM.
Les tailles et offsets utilisés sont contrôlés à la compilation. Le build
n'active ce shim qu'après vérification de `libheap.a` et de ses en-têtes.
Sans cette preuve, la requête publique ESP-IDF reste utilisée, avec son
coût habituel ; aucune application n'est bloquée pour cette seule raison.

`Fit` est une photographie de la possibilité d'allouer une taille précise,
pas une réservation physique ni une mesure du plus grand bloc. L'allocation
réelle reste l'arbitre final. Les informations de diagnostic distinguent les
maxima mesurés des réponses `>=`, `<` et `?` pour la taille demandée.

Les résultats hôtes utilisent ASan et UBSan, sauf le contrôle d'alignement :
le TLSF Espressif emploie un alignement de blocs de 4 octets, tandis que les
pointeurs de l'hôte LP64 nécessitent normalement 8 octets. Le code upstream
n'a pas été modifié pour l'essai. LSan est désactivé à cause des restrictions
`/proc` du conteneur. Ces limites ne concernent pas la compilation cible
32 bits, et aucune latence physique du P4 n'est déduite de ce test.

Les parcours complets restent disponibles pour les diagnostics explicites.
Ce lot supprime ceux du chemin d'admission dans la configuration auditée ;
il ne prétend pas supprimer tous les parcours de heap de toutes les
bibliothèques ou toutes les politiques adaptatives du système.
