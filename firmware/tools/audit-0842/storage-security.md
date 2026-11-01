# Stockage, paramètres et secrets — audit 0.8.42

## État initial (avant modifications)

Sources : livraison vérifiée 0.8.41, renommée 0.8.42 ; empreintes et tailles dans `storage-baseline.json`. Aucun essai Tab5 physique. Les temps, fragmentation et erreurs électriques SD restent à mesurer sur appareil.

- `Tab5Storage` : leases atomiques, récupération exclusive, trois tentatives au maximum, hors demandes manuelles ; jamais de démontage tant que leases ou fichiers SD ouverts restent actifs. Reprise sur erreur EIO/ENODEV/ENXIO/ETIMEDOUT/EBADF ; une panne ENOSPC ne provoque pas de remontage.
- `Tab5VolumeFS` : générations invalident les handles après démontage ; `/USB` est routé séparément. `Tab5StorageUse` protège contre démontage, pas une transaction complète de deux applications sur un même chemin. FAT/VFS sérialise les opérations natives ; aucun verrou de remplacement interapplications n’a été identifié.
- `WriterStorage` : sauvegarde incrémentale 4 Kio, snapshot PSRAM, vérification du temporaire avant remplacement et restauration du précédent sur échec. Chargement publié seulement après validation complète.
- `Tab5FileOperation` : worker, manifeste borné, destination O_EXCL, fsync, relecture/comparaison et contrôle source avant suppression d’un déplacement. `Tab5UsbStorage` : aucun formatage au montage, formatage explicitement demandé uniquement, leases/générations et synchronisation cache à l’éjection.
- Paramètres `.ino` : lecture dans une structure candidate, contrôle fin de fichier/taille, distinction ENOENT/erreur, protection contre écrasement des paramètres après échec de lecture. Sauvegarde temporaire/bak ; ce module appartient à l’audit principal.
- Coffre : NVS pour les enveloppes de clés/PIN, deux banques AES-GCM sur SD pour les secrets, table en PSRAM (36 044 octets, 64 entrées). Ancienne banque active conservée tant que la nouvelle n’est pas relue/authentifiée. Aucun format de chiffrement ni migration automatique ajoutés.
- Cryptographie : PBKDF2 historique 600 000 itérations, annulation et yield, authentification de toutes les tranches avant publication du clair. Les buffers PSRAM cryptographiques sont alignés sur lignes de cache distinctes. Les clés sont effacées des buffers temporaires.

## Anomalies confirmées à l’inspection

1. **Intégrité, élevée :** `Tab5Secrets::load` assimilait deux `exists()` faux à un coffre vide sans contrôler errno. Un EIO pouvait donc faire accepter puis enregistrer une table neuve au-dessus des banques existantes. Une seule banque indéterminée pouvait aussi masquer la banque la plus récente. L’archivage de récupération explicite avait la même confusion pour le répertoire des secrets, permettant de supprimer la clé après un archivage non réalisé.
2. **Réactivité, élevée :** la boucle graphique appelle `KeyStore::unlocked()` à chaque tour ; ce simple état prend un mutex avec attente illimitée. Le worker `openDeviceImpl` garde ce mutex pendant NVS, vérification PIN, crypto et persistance. L’interface peut donc attendre le worker malgré son exécution asynchrone.
3. **Réactivité, localisée :** enregistrer/supprimer un secret déclenche directement chiffrement, écriture puis relecture complète de la table depuis le callback graphique. Cela explique un blocage à cette action ; cela ne démontre pas un lag permanent au repos.

## Limites et vérifications restantes

Les tensions/bus SPI, latences de carte, retrait brutal en pleine écriture, coupure pendant renommage FAT, durabilité effective de flush/fsync et révisions matérielles nécessitent des essais physiques. Aucune garantie d’isolation entre code natif/ELF n’est fournie : une application native dans le même espace mémoire peut accéder aux données de l’OS. Le mode appareil protège les données retirées de la SD, pas un accès privilégié au NVS non chiffré de l’appareil.


## Corrections appliquées

- **Secrets / intégrité élevée** : distinction stricte entre ENOENT et erreur de présence, rejet d’une lecture de banque échouée avec errno, cache d’échec conservé jusqu’à une nouvelle tentative explicite. L’archivage de récupération n’autorise plus la suppression du maître NVS si l’existence du répertoire source reste indéterminée. Les deux banques et tous les formats restent inchangés.
- **KeyStore / réactivité élevée** : les seules sondes d’état `unlocked`, `usingDevice`, `credentialsAccessible` et `Secrets::ready` utilisent le mutex FreeRTOS avec attente zéro. Elles indiquent indisponible si occupé ; les accès effectifs aux clés et secrets reprennent toujours le verrou normal et vérifient l’autorisation. Aucun secret ne devient accessible du fait de cette optimisation.
- **AtomicFile / intégrité transversale** : `tab5AtomicProbe` distingue absence certaine et erreur. `tab5AtomicReplace` vérifie primary/backup, ne supprime une ancienne sauvegarde que si le primaire est confirmé, et restaure le primaire si la promotion échoue. Si la restauration échoue aussi, le backup reste présent. Les consommateurs conservent leurs noms et formats existants.
- **Thèmes, récents, visibilité** : `Tools::readJson` et `HiddenFiles::load` n’acceptent plus une erreur d’ouverture suivie d’une erreur de présence comme document JSON vide modifiable.
- **Éditeur / projets et documents** : ajout d’un résultat de présence explicite au petit adaptateur `FileSystem`. L’adaptateur SD utilise errno. Les projets ne sont déclarés neufs que lorsque tous les fichiers de récupération sont réellement absents. Une erreur de lecture interdit un commit qui remplacerait l’état inconnu ; une nouvelle lecture complète rétablit le fonctionnement normal. Le chargement documentaire n’utilise plus une ancienne sauvegarde quand la présence du primaire est inconnue. Les sauvegardes contrôlent aussi les probes avant remplacement. Le nom d’un brouillon n’est choisi que si primaire et deux variantes de sauvegardes sont absents.
- **Réglages, document simple, progression lecteur (.ino)** : promotion par le helper commun, contrôle longueur/flush/getWriteError/fermeture avant commit. La progression est lue dans des variables candidates et publiée après lecture complète et validation taille/position ; une erreur de read quitte immédiatement la fonction, au lieu de reboucler sur available. Les backups `.bak` sont conservés et la progression peut être relue depuis le backup si le primaire manque. La migration d’un signet ne supprime plus la destination avant de tenter son remplacement.

Le SDK effectivement compilé ignore le résultat de `fflush`/`fsync` dans `File::flush` et celui de `fclose` dans la fermeture VFS. Les écritures corrigées remettent errno à zéro et le contrôlent immédiatement après ces appels ; elles ne déduisent pas la réussite de getWriteError seul. Le contrat `finishWrite` transmet ces erreurs à SaveJob/Workspace avant promotion. La même vérification couvre les exports coffre et la copie d’image de l’éditeur. Les tests de faute flush ne modifient volontairement pas le flag write-error, conformément à cette frontière SDK.

Les choix/délais d’autosauvegarde n’ont pas changé. Aucune nouvelle tâche ni sauvegarde automatique d’incident n’a été ajoutée. Aucun format de clé, chiffrement, partition ou fichier utilisateur n’a été migré par ces corrections.

## Bilan disponible avant/après

| Mesure de laboratoire | 0.8.41 | 0.8.42 |
| --- | --- | --- |
| Erreur stat sur 2 banques / 1 banque secrets | Acceptée comme cache neuf/ancien | Échec fermé, octets conservés |
| Erreur stat du répertoire à archiver | Maître supprimable sans archivage | Archivage échoue, maître conservé |
| Sondes état pendant mutex worker retenu | Attend la libération, échoue à la fenêtre 100 ms | Répond avant libération, autorisation fausse |
| Erreur open + stat JSON/visibilité | Document vide accepté | Échec explicite |
| EIO sur catalogue projets | Liste vide modifiable | Ancienne liste conservée, commit bloqué |
| EIO primaire document, backup présent | Ancien backup chargé | Chargement échoue, document RAM conservé |
| Lecture progression interrompue à chaque octet | Boucle sans avancement détectée sur 62 points | 62 échecs immédiats, aucune publication partielle ni boucle |

La fenêtre de contention de 100 ms est une épreuve d’ordonnancement contrôlée de l’hôte, pas une mesure de latence Tab5. Aucun chiffre de gain SRAM, PSRAM, CPU ou démarrage n’est revendiqué pour ce lot. Les tables et piles restent de même taille : cache secrets PSRAM 36 044 octets ; crypto par tranches de 4 Kio ; worker coffre 16 Kio, copie fichiers 12 Kio, formatage USB 8 Kio. Les nouvelles sondes utilisent le mutex existant et ne créent aucun buffer permanent ni polling supplémentaire. Les contrôleurs progression utilisent un buffer local borné de 1 280 octets, à inclure dans la marge de pile UI mesurée sur appareil.

## Vérifications exécutées

- `vault-0842-baseline.json` et `vault-0842.json` : modules de production avec vraie mbedTLS 3.6.5, commit Espressif `ffb280bb63c78bfec1e1ab55040671768c85c923`. 1 118 contrôles, 95 injections d’opérations de fichiers, 40 injections récupération NVS/fichiers, 50 cycles coffre, erreurs flush/close explicites préservant les banques. Authentification GCM, PBKDF2 600 000, formats portables/historiques, PIN, annulation, révocation et absence de clair dans les fichiers contrôlés. NVS, fichiers et lancement de tâches simulés aux frontières matérielles ; sondes de contention avec véritables threads/mutex hôte.
- `atomic-storage-0842-baseline.json` et `atomic-storage-0842.json` : helper réel, lecteur JSON Tools extrait sans modification, préférences visibilité réelles et vrai ArduinoJson. Quatre reproductions EIO avant/après et 40 fautes transactionnelles. Absence réelle et restauration d’un commit interrompu restent fonctionnelles.
- `writer-storage-0842-baseline.json` et `writer-storage-0842.json` : vrais modules Workspace/Storage/Document/Encoding avec ASan/UBSan. Trois reproductions EIO corrigées, 80 injections commit catalogue, 40 injections sauvegarde documentaire, synchronisation en échec avant promotion ; récupérations de backup et publication RAM contrôlées.
- `persistence-0842-baseline.json` et `persistence-0842.json` : 985 contrôles de réglages/migration, 62 interruptions de lecture progression à chaque octet, écriture courte, flush et fermeture en erreur, deux échecs de rename de migration, 30 injections commit progression. Ancienne boucle détectée par un garde de test après 4 096 appels available sans progression ; nouveau code sort immédiatement sur read négatif. Fonctions .ino extraites sans modification, ASan/UBSan ; aucune sauvegarde automatique supplémentaire.
- `storage-volume-0842.json` : cycle de vie réel des wrappers FileImpl et leases, références partagées, générations après retrait, exclusion de récupération, SD/USB indépendants et révisions de rafraîchissement publiées une seule fois à fermeture. Adaptateur VFS matériel simulé.
- Compilations ciblées P4/SDK réel : `storage-target-0842.json` et `storage2-target-0842.json`. La compilation complète finale est décrite dans le rapport principal.

Les runners Linux reproductibles sont sous `tools/vault-tests-0842`, `atomic-storage-tests-0842`, `writer-storage-tests-0842`, `persistence-tests-0842`. Les anciens rapports de versions précédentes sont conservés. LeakSanitizer ne peut pas énumérer les threads `/proc` de ce conteneur : il est désactivé explicitement ; les tests ASan/UBSan ne constituent donc pas une preuve d’absence de fuite sur la carte.

## Résiduels et essai court sur appareil

1. **Moyenne, réactivité ponctuelle :** Enregistrer/Supprimer un secret chiffre et relit encore la table sur la tâche UI. Le défaut d’attente globale sur le worker est supprimé ; ce coût local reste à chronométrer et à déplacer vers un worker si significatif. Pas de changement de protocole UI introduit sans tests complets de fermeture/annulation.
2. **Moyenne, concurrence même chemin :** les leases bloquent le démontage, pas une modification concurrente du même document par deux applications. Les remplacements sont vérifiés et conservent une sauvegarde ; ils ne fournissent pas une transaction isolée entre toutes les applications. Tester copie/HTTP/éditeur vers des noms distincts puis explicitement vers le même nom, conserver journaux et les deux versions pour analyse.
3. **Matériel, potentiellement élevée :** retirer la carte pendant lecture/copie (sur fichiers de test copiés), vérifier erreur explicite, accueil réactif et absence de remontages incessants ; réinsérer puis lancer le retest SD et relire les documents. Une coupure pendant FAT/flush ne peut pas être simulée fidèlement ici : réaliser uniquement sur copie de carte, vérifier primaire et backup après redémarrage.
4. **Matériel, réactivité :** déplacer la souris pendant ouverture PIN/coffre et pendant écriture d’un secret de test. Capturer journal série, état mémoire interne/PSRAM et minima de piles avant/après 20 ouvertures/fermetures ; aucune valeur de clé/PIN à journaliser.
5. **Sécurité architecturale :** code ELF/natif et OS partagent l’espace mémoire. NVS et coffre restent au format existant ; cette correction ne prétend pas ajouter une isolation matérielle ou protéger un maître NVS contre une extraction privilégiée de la Flash.
