# Meshtastic 0.8.41 : mémoire et réglages

Le refus de démarrage de la version 0.8.40 est reproductible avec le profil annoncé : 275 Kio occupés sur 347 Kio laissent 72 Kio. Après création de la pile du worker de 8 Kio, son admission de 48 Kio, ajoutée à la marge globale de 32 Kio, ne peut pas passer. La réservation de création du worker est déjà libérée ; elle n'est pas comptée deux fois.

## Allocation du SDK

Le SDK M5Stack 3.3.9 utilise ESP-IDF v5.5.4, commit `735507283d`, et compile NimBLE avec l'allocateur INTERNAL. ESP-IDF propose également un mode EXTERNAL pour les allocations génériques de l'hôte NimBLE. La version 0.8.41 compile localement le seul module d'allocation officiel dans ce mode :

- `Tab5MeshAllocatorEspIdf554.inc` est une copie intacte de `components/bt/porting/mem/bt_osi_mem.c` de ce commit, également identique au tag v5.5.4. SHA-256 : `0179444c4143dfd3155c94b6dcee0c3a4877844d93d3d3993fb54b138ec11f2b`.
- `Tab5MeshAllocator.c` sélectionne EXTERNAL seulement pour ce module. Il ne modifie ni l'archive SDK, ni `malloc/calloc` globaux, ni les paramètres de compilation du reste du SDK.
- Les six définitions locales satisfont les références avant extraction du membre `libbt.a(bt_osi_mem.c.obj)`. Le contrôle de publication vérifie la provenance dans la carte de liens ; les fonctions inutilisées peuvent ensuite être éliminées par le linker.
- Les allocations génériques de l'hôte et ses pools demandent `SPIRAM|8BIT`. Les variantes `*_internal` conservent `INTERNAL|8BIT|DMA`. Les piles restent créées par FreeRTOS. L'archive du transport Hosted 2.12.8 ne référence aucun de ces six allocateurs : ses propres buffers DMA ne sont pas redirigés.
- Le remplacement est limité au profil P4/Hosted et à la version exacte vérifiée, sans statistiques/debug mémoire supplémentaires. Un autre SDK utilise son allocateur natif, sans symboles de remplacement ni erreur bloquant la compilation.

Le fichier `Tab5MeshAllocator-LICENSE.txt` conserve la licence Apache 2.0 d'ESP-IDF ; le source copié conserve ses mentions SPDX.

Sources : [module officiel au commit du SDK](https://github.com/espressif/esp-idf/blob/735507283d/components/bt/porting/mem/bt_osi_mem.c), [options NimBLE officielles](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/kconfig-reference.html#config-bt-nimble-mem-alloc-mode).

## Admission et limites

| Charge après création du worker | 0.8.40 | 0.8.41, SDK vérifié |
| --- | ---: | ---: |
| Budget interne | 48 Kio | 28 Kio |
| Plus grand bloc interne requis | 10 Kio | 6 Kio |
| Budget PSRAM | 0 | 44 Kio |
| Plus grand bloc PSRAM requis | 0 | 10 Kio |
| Marge globale interne | 32 Kio | 32 Kio |
| Marge globale PSRAM | 512 Kio si demandée | 512 Kio |

Ces budgets sont des estimations d'admission conservatrices, pas une mesure de consommation sur tablette. La réduction interne correspond au déplacement effectif des pools vers la PSRAM. L'ancienne provision de 20 Kio pour objets, NVS, métadonnées et transitoires reste incluse dans le budget interne ; une provision de 20 Kio est également comprise dans le budget PSRAM. Aucune marge globale n'est réduite. La pile worker de 8 Kio et celle de l'hôte de 5 Kio restent internes.

Les erreurs de PSRAM et de fragmentation PSRAM sont distinguées des erreurs de RAM interne. La RAM INTERNAL peut inclure les portions admissibles de SRAM, TCM/SPM et LP : ce n'est pas la capacité SRAM physique du composant.

Les assertions fatales `malloc`/NPL du SDK sont conservées. Les tests ne prétendent pas couvrir tous les chemins d'échec partiel internes du SDK. La réserve préalable couvre les pools et les marges, mais ne transforme pas un échec concurrent du matériel ou du SDK en garantie universelle de récupération.

## Interface

`openSettings(returnScope)` ouvre les options depuis Réglages sans ouvrir les messages, créer leur modèle ou démarrer Bluetooth. Le popup utilise les couleurs et la police de l'OS, prend le focus clavier et rend le focus au bouton d'origine après fermeture. La fermeture de cette seule fenêtre ne coupe pas une radio déjà active. `visible()`, `scope()` et `key()` incluent cette fenêtre pour le routage des événements par l'OS.

L'état `Badge.active` demande un worker actif, le statut `Ready`, une configuration Meshtastic terminée et l'absence d'arrêt demandé. Une synchronisation Bluetooth ou une recherche de périphériques ne suffit plus à rendre la pastille active. Le shell affiche la pastille uniquement dans cet état.

## Vérification

- `meshtastic-tests-0841/run.py` : protocole, stockage, callbacks de production et LVGL réel avec ASan/UBSan ; allocator SDK compilé en C, observation des capacités, profil de refus précédent, budgets séparés et erreurs d'allocation.
- Les cycles d'échec de pool vérifient les allocations et libérations pilotées par le test, pas tout le démarrage interne de NimBLE. Les erreurs d'initialisation et de création de tâche vérifient séparément le nettoyage du cycle de vie applicatif avec frontières ESP/NimBLE simulées.
- `meshtastic-target-0841.py` : compilation réelle P4 des sources Mesh, Store et allocator avec les en-têtes du SDK livré.
- `verification/meshtastic-sdk-0841.json` : version et empreintes SDK, copie du module, six exports cible, absence de références Hosted à l'allocateur remplacé. La preuve de lien complet relève du contrôle de publication.

Aucun appareil Meshtastic physique n'a été testé. Restent à mesurer sur tablette : pics et fragmentation réels, répétition démarrage/arrêt, coexistence Wi-Fi/Bluetooth, appairage et échanges radio avec le firmware C6 installé.
