# Catalogue d’applications et exclusions héritées — 0.8.43

## Référence et diagnostic initial

Référence : sources de l’archive livrée 0.8.42, conservée intacte (`c7e5fc18494b2cf8be7ad91527d2da6a053717e8138902404e2030e4e54cd927`). Les fichiers nécessaires aux reproductions avant/après sont copiés sans modification dans `tools/catalogue-tests-0843/baseline-0842`. Ce dossier sert seulement aux tests hôte, pas à la compilation du firmware.

Anomalies confirmées dans le code initial :

| Constat | Effet |
|---|---|
| Blacklist des noms `Quake.app.elf` et `Duke3D.app.elf` dans le catalogue | Ces fichiers présents sur SD/USB n’étaient pas proposés, indépendamment de leur contenu et des choix de l’utilisateur. |
| Exclusion du chemin `/Applications/AtelierImage.app.elf` | L’ancien binaire n’était pas visible même lorsqu’un utilisateur le conservait volontairement. |
| Refus supplémentaire de quatre noms Quake/Duke3D dans `tab5ExecutableRun()` | Ouvrir ces fichiers depuis l’explorateur n’évitait pas la restriction. Le chargeur retournait « Application retirée de cette version » avant toute validation réelle du fichier. |
| Filtre générique `_` et exclusion de tous les fichiers de premier niveau dans les racines MicroPython | Un ELF ordinaire `_tool.elf`, un paquet `_bundle/main.elf` ou un ELF dans une racine MicroPython déclarée pouvait aussi manquer. |
| Un échec d’ouverture d’un `main.elf` optionnel équivalait à un fichier absent | Une erreur EIO pouvait publier un catalogue incomplet alors qu’un catalogue valide était déjà affiché. |
| Le pas de scan exécuté par l’UI faisait les E/S | Un pas pouvait ouvrir une entrée puis jusqu’à trois fichiers `main.*`. Le scan Python peut effectuer jusqu’à huit `readdir`, des `stat` et la vérification d’un alias. Le nombre de pas est borné, mais pas la durée réelle d’une lecture SD. |

La reproduction sur la source 0.8.42 complète découvre **0 des 7 ELF** de la fixture précisément concernée par ces filtres. Les quatre noms de la blacklist du chargeur sont refusés avant entrée du worker. Cela établit la restriction de politique ; ce n’est pas une mesure de performances de la Tab5.

## Corrections de politique

Les exclusions nominatives du catalogue et du chargeur sont supprimées. La validation habituelle du chemin, du suffixe, du fichier ELF, de l’architecture, des limites et de la relocation reste en place. L’acceptation d’une demande ne signifie pas qu’un binaire incompatible a démarré. Les ELF natifs partagent toujours la mémoire du firmware : aucune isolation matérielle n’est revendiquée.

Le filtre Python privé est limité aux scripts/modules concernés. Un ELF ordinaire n’est plus exclu parce que son nom commence par `_`, ni parce qu’il est dans l’une des racines MicroPython déclarées. Les fichiers commençant par un point et les modules Python privés ne sont pas présentés comme applications. Les bibliothèques Python ne sont pas parcourues récursivement.

Les préférences explicites d’épinglage, de masquage, d’ordre et d’icône conservent les mêmes clés et le même format NVS de 1 032 octets. La migration du format précédent est utile aux données utilisateurs et reste en place ; seuls ses commentaires nommant inutilement une ancienne version ont été simplifiés. Aucun fichier utilisateur, binaire ou choix n’est supprimé ou réinitialisé. Aucun jeu absent de la distribution n’est ajouté artificiellement.

Un échec d’ouverture autre qu’ENOENT pendant la découverte d’un paquet marque le scan en erreur. Le dernier catalogue complet reste publié, plutôt que de faire disparaître une application lors d’une panne de lecture.

## Propriété du scan et exécution

Le scan utilise une tâche ponctuelle FreeRTOS, cœur 0, priorité 1, pile interne de 8 Kio. Le cœur UI reste propriétaire du tableau publié, des préférences, des icônes et de LVGL. Le worker ne crée ni ne modifie d’objet LVGL.

- L’UI prépare le tableau de travail PSRAM à partir des applications intégrées.
- L’admission optionnelle utilise `Manager::reserve()` directement : pas de boucle de récupération de 60 ms ni de purge de caches pour cette tâche facultative. Le budget standard représente 8 Kio de pile et 4 Kio de marge de création, en conservant les marges globales du gestionnaire.
- Le worker possède les répertoires, fichiers et baux du scan. Il remplit uniquement le tableau de travail.
- Après fermeture de tous ces objets et le relevé de pile, il publie un résultat puis la fin avec un ordre mémoire release. Il ne touche plus au catalogue après cette publication.
- `scanStep()` côté UI constate la fin par acquire, compare le contenu et échange les tableaux uniquement si le scan a réussi et le contenu a changé.
- `endScan()` demande une annulation coopérative. Il ne joint pas la tâche et ne ferme pas ses fichiers depuis l’UI. Une annulation avant l’entrée de la tâche évite toute ouverture SD.
- Une nouvelle demande pendant l’annulation remplace l’intention précédente et est acceptée. Le premier `scanStep()` après l’acquittement démarre cette dernière demande ; aucun délai de cinq secondes n’est requis pour ce cas.

Si l’admission ou la création de tâche échoue, la découverte conserve son ancien fonctionnement incrémental sur l’UI et émet un diagnostic série. Ce repli préserve la fonctionnalité lorsque la mémoire interne disponible ne suffit pas à une tâche supplémentaire ; il ne donne pas une garantie de latence SD dans cette situation. Aucun worker permanent n’est ajouté.

Le worker cède un tick après une tranche de huit millisecondes de parcours en cache afin de permettre l’exécution de l’idle/watchdog. Ce mécanisme ne remplace pas la publication ni l’annulation et ne retarde pas la tâche UI. Un appel pilote déjà bloqué ne peut être interrompu proprement par cette annulation.

## Mémoire et limites

Les deux tables existantes restent bornées à 64 descripteurs de 384 octets sur P4 : 24 Kio PSRAM pour le tableau publié et 24 Kio pour le tableau de travail, alloué au premier scan puis réutilisé. Aucune copie de ces tables ni buffer de document supplémentaire n’est ajouté. Le cache d’icônes existant reste plafonné à 192 Kio PSRAM. L’arrêt des scans sur l’accueil natif, traité dans le lot accueil, permet de ne pas allouer la seconde table tant qu’une bibliothèque n’en a pas besoin.

La pile du worker et son TCB sont transitoires. Après publication, l’épilogue et la suppression FreeRTOS peuvent encore garder brièvement l’ancienne pile allouée ; l’admission d’un nouveau travail voit cette consommation réelle et peut choisir le repli. Le relevé `tab5TaskCheckpoint("catalogue")` permet de mesurer la marge réelle sur appareil. Le plus grand cadre GCC du scanner est de 1 168 octets ; cette valeur n’est pas la profondeur cumulée de sa pile de 8 Kio.

La limite de 64 applications et 384 entrées par scan reste inchangée. Les applications au-delà de ces limites restent accessibles dans Fichiers. L’admission d’une tâche ne transforme pas la PSRAM en SRAM interne ou DMA.

## Validation réellement exécutée

Preuves : `tools/verification/catalogue-0843.json`, `catalogue-target-0843.json` et les journaux associés.

- Catalogue de production complet compilé sur hôte avec doubles uniquement aux frontières FS/NVS/LVGL/RTOS, ASan et UBSan actifs. La fixture des sept noms/chemins passe de 0/7 à 7/7.
- Fonctions réelles du chargeur : quatre anciens noms bloqués désormais acceptés, lecture toujours différée et annulation propre ; les 769 scénarios de chargement/erreur/cycle de vie du lot précédent repassent sur le code courant.
- Ajout, suppression et ordre de répertoire modifié ; publication atomique ; choix utilisateur explicitement masqué maintenu ; scans annulés ou en EIO ; échec d’allocation du staging ; limites 64/384 ; SD et USB séparément.
- Worker réel `std::thread` en test hôte, E/S retenues par une barrière : 1 000 appels UI de `scanStep()` continuent sans aucun `open` sur l’UI. L’annulation revient alors que l’E/S reste retenue. La dernière demande redémarre seulement après fermeture du propriétaire précédent.
- Annulation avant entrée : zéro ouverture, côté UI et worker. Reset du catalogue pendant l’E/S : l’ancienne tâche ne remplace pas les nouvelles applications intégrées. Retrait simulé du support et erreur de lecture : catalogue publié conservé.
- Création de tâche refusée : le repli incrémental découvre encore les nouveaux ELF. Cent cycles du worker se terminent sans tâche hôte active, fichier ou bail retenu et sans nouvelle allocation des tables PSRAM.
- Compilation des objets ESP32-P4 sur le vrai SDK M5Stack 3.3.9/GCC 14.2 : catalogue et chargeur réussis, empreintes de sources finales vérifiées.
- Revue indépendante du protocole de propriété/publication par le lot mémoire, sans nouvelle course ou libération anticipée confirmée.

Ces essais n’exécutent pas les binaires Quake/Duke3D, ne mesurent pas la carte SD physique et ne valident pas la planification multicœur réelle de l’ESP32. LeakSanitizer est désactivé dans l’environnement ptrace ; ASan/UBSan et les compteurs explicites de tâches/fichiers/baux sont utilisés. La cause physique des flashs turquoise reste à investiguer par les compteurs écran et les essais sur Tab5.
