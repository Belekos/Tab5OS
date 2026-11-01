# Ressources, attentes natives et coexistence — 0.8.47

La base est l'archive livrée 0.8.46, SHA-256 `22d99628cc3ab6a9673be1886755ec9a3092aaede3f017a7e0e20443cc0816a6`. Les flashs et claquements rapportés sur appareil restent des régressions réelles ; cette revue ne leur attribue pas une cause unique sans mesure matérielle.

## Modifications réalisées

Le verrou global d'admission mémoire utilisait un `atomic_flag` et huit essais espacés d'un tick. Un demandeur prioritaire pouvait réveiller régulièrement le processeur sans transmettre sa priorité au détenteur. Il utilise désormais un mutex FreeRTOS statique, avec héritage de priorité et attente bornée à 8 ms. L'initialisation est explicite sur la tâche de démarrage **avant** `Tab5DebugSerial.begin`, car ce dernier crée déjà le worker série et demande une admission. Aucune création de verrou dans un constructeur global, aucune allocation de heap et aucune nouvelle tâche.

Un worker refusé demandait une collecte à l'UI puis se réveillait à chaque tick en attendant sa fin. Chaque demande utilise maintenant son bit de complétion dans un groupe d'événements statique. Le worker dort jusqu'au résultat ou à son timeout. Les notifications applicatives des tâches restent disponibles : le SDK n'offre qu'un emplacement par tâche. Le démarrage générique de `Tab5Task.h` n'a donc pas été converti en consommation de cette notification unique.

La propriété des demandes est conservée : `Free → Building → Pending → Processing → Done`. Si le demandeur expire pendant un callback, il passe à `Abandoned` ; seul le collecteur libère alors l'emplacement après le retour réel du callback. Un événement tardif peut provoquer un réveil superflu après réutilisation d'un emplacement, mais le demandeur vérifie toujours son état avant d'admettre le travail. Il ne peut pas prendre la fin d'une ancienne collecte pour la fin de la sienne.

La collecte reste sur la tâche UI, hors rendu, puisque ses callbacks détruisent parfois des objets LVGL. Les caches du propriétaire au premier plan et du demandeur ne sont pas expulsés. Les marges **32 Kio internes et 512 Kio PSRAM**, les vérifications de plus grand bloc et les quotas futurs sont inchangés. Les quotas ne représentent pas des octets encore alloués : `allocated` les réduit une fois les allocations visibles dans le heap.

## Écriture masquée

L'application libérait déjà sa géométrie de rendu, ses images dérivées et son tampon de console en quittant la page. Dessin libérait déjà son aperçu RGB565. Ces gains antérieurs ne sont pas annoncés comme nouveaux. Les documents, calques, sélections et historiques d'annulation restent conservés.

Écriture continuait cependant à indexer ses deux documents structurellement à chaque tour de boucle même masquée. Ce travail est désormais suspendu lorsqu'elle est cachée, puis reprend à la position exacte atteinte lorsqu'elle redevient visible. Les enregistrements explicites en cours, l'autosauvegarde selon le choix de l'utilisateur et les transactions de session restent servis.

Un collecteur **uniquement sous pression mémoire** peut également libérer ses deux tableaux de checkpoints lexicaux. Les textes, couleurs déjà affichées, positions et annuler/rétablir ne sont pas détruits. Ces tableaux sont reconstruits à la prochaine utilisation visible. Leur allocateur sait échouer proprement : le contexte syntaxique indisponible utilise le comportement existant de coloration simple.

Les tableaux de l'index structurel ne sont volontairement pas expulsés par ce nouveau collecteur. Leur `ExternalAllocator` termine actuellement le programme sur un échec d'allocation dans une construction sans exceptions. Favoriser leur reconstruction sous pression aggraverait ce défaut. Cette dette nécessite une structure d'index optionnelle à allocation récupérable, pas la suppression des marges mémoire.

## Mesures vérifiables

| Élément | 0.8.46 | 0.8.47 | Nature |
|---|---:|---:|---|
| `sizeof(Manager)` ESP32-P4 | 820 octets | 960 octets | Compilation du véritable type avec le compilateur cible ; **+140 octets internes statiques** |
| Un tableau lexical P4 | 22 616 octets | libérable sous pression | Taille calculée par le compilateur cible ; PSRAM, seulement si alloué |
| Deux tableaux lexicaux | 45 232 octets conservés | libérables | 44,17 Kio PSRAM au maximum pour ces deux tableaux, pas un gain SRAM |
| Réveils d'attente avant une collecte retardée dans le test hôte | 25 | 0 | Un appel natif d'attente d'événement dans le cas corrigé ; **pas une mesure de latence Tab5** |
| Cycles cache lexical / reprise / historique | — | 100 | Documents et sélections conservés, dix injections d'échec d'allocation, zéro octet de modèle suivi en fin de cycle |

Les objets statiques FreeRTOS du SDK ont été mesurés par compilation : mutex 92 octets, groupe d'événements 40 octets. La variation complète du gestionnaire inclut handles et alignement ; utiliser le delta de 140 octets, pas une addition approximative des seuls champs.

Sur hôte 64 bits, les deux tableaux lexicaux font 49 344 octets. Ce résultat de test ne doit pas être substitué aux 45 232 octets de la cible 32 bits. Le test suit l'allocateur Writer PSRAM ; il ne mesure pas les allocations ordinaires du STL hôte ni le heap physique au repos.

## Organisation des tâches et limites conservées

Le second cœur P4 est déjà actif. Le cœur 0 porte notamment décodage audio priorité 5, Speaker priorité 6, vidéo priorité 5, NimBLE priorité 5, USB, interprète et chargeur ELF. Cartes et récepteur radio utilisent le cœur 1 avec l'UI. Déplacer indistinctement les traitements vers le cœur 0 ferait concurrence à l'audio ; un découpage ou une mise en sommeil native est préférable pour les tâches sans travail.

Les deux cœurs partagent les accès aux mémoires et périphériques. Un worker de persistance peut éviter un long callback UI ; il ne supprime pas, à lui seul, la suspension globale provoquée par une opération Flash avec cache désactivé. Une pile exécutant NVS reste interne sur ce SDK. Le C6 dispose de son propre firmware et de sa mémoire : ce n'est pas un heap additionnel du P4.

Cartes ferme déjà ses connexions et rend son canvas en quittant la page ; le retrait d'une tâche peut attendre le nettoyage idle de FreeRTOS. L'audio possède déjà un retrait de worker inactif en 0.8.46. Les applications ou services explicitement lancés en arrière-plan ne sont pas arrêtés arbitrairement lors de l'ouverture de Média.

## Dettes restant bloquantes pour une qualification de production

| Criticité | Dette établie | Suite nécessaire |
|---|---|---|
| Haute sous épuisement PSRAM | Certains conteneurs utilisent un allocateur qui appelle `abort` sur OOM | Rendre récupérables les structures dérivées avant d'étendre les expulsions ; tests d'erreur au niveau de l'application entière |
| Haute si reproduite | Attentes de fermeture pilotes NimBLE/USB/DMA sans borne après erreur | Traitement d'arrêt à états avec propriété du transfert ; ne jamais libérer un buffer encore détenu par le pilote |
| Moyenne, certaine sous pression | Admission appelée depuis l'UI : collecte synchrone puis jusqu'à 60 ms pour un retour de pile | Transformer les opérations de lancement concernées en continuation ; la suppression de l'attente seule produirait des refus prématurés |
| Moyenne | Après collecte terminée, observation par tick du retour des stacks/quotas ; file de demandes pleine | Aucun événement natif de ce gestionnaire ne signale le nettoyage de pile idle. La boucle est bornée et conserve sa fonction ; elle n'est pas déclarée supprimée |
| Moyenne | Certaines sauvegardes, transformations et décodages d'image restent synchrones dans l'UI | Réutiliser les jobs existants et publier leurs résultats avec propriété explicite |
| Haute, limite d'architecture | ELF natif partage l'espace mémoire privilégié | Ne pas annoncer d'isolation matérielle absente ; les applications externes restent du code de confiance |

## Preuves et reproduction

- `tools/memory-tests-0847/run.py` : noyau réel du gestionnaire compilé en branche Arduino avec frontières FreeRTOS adaptées ; complétion avant sommeil, huit demandeurs, expiration, abandon en cours de callback, contention, puis tests historiques d'admission et 400 cycles. Rapport `tools/verification/memory-0847.json`.
- `tools/writing-cache-tests-0847/run.py` : méthodes réelles de service/collecte, vrais moteurs Document/RichText/Index/SyntaxIndex ; périphériques et actions UI sans rapport remplacés aux frontières. Rapport `tools/verification/writing-cache-0847.json`.
- `tools/audit-0847/probe_resource_sizes.py` : tailles des véritables types à partir de symboles ELF P4. Rapport `tools/verification/resource-sizes-0847.json`.
- `tools/verification/resource-target-0847.json` : compilation des unités modifiées avec le SDK P4 épinglé. La compilation complète finale reste la preuve d'intégration.
- `tools/verification/resource-race-independent-0847.json` : revue indépendante et 32 contrôles sur huit cycles forçant une complétion tardive après réutilisation d'emplacement. Le noyau Arduino réel rejette l'ancien signal comme simple réveil superflu et attend la fin de sa propre collecte.

ASan et UBSan sont actifs dans les tests hôte. LeakSanitizer est désactivé en raison des limites d'inspection du processus de cet environnement ; les allocations Writer suivies reviennent à zéro. Aucun chiffre de CPU, de fragmentation physique, de pile minimale ou de disparition des flashs n'est déduit de ces tests.
