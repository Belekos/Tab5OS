# Relecture indépendante stockage et persistance — 0.8.45

Périmètre : `Tab5VolumeFS.cpp`, backport SDK `prepare_fs_errno.py`, charge/publication des lieux cartographiques et fonctions du sketch `probeSdWrite`/`serviceSdRecovery`. Relecture statique indépendante ; les exécutions des méthodes SDK et les injections de fautes sont consignées dans les rapports du propriétaire stockage. Ce document ne constitue pas une exécution matérielle ou un test supplémentaire.

## Points confirmés

- Le SDK peut retourner un `shared_ptr<FileImpl>` non nul dont le descripteur réel est invalide. `openedFile` vérifie maintenant la validité réelle, détruit cet objet invalide et préserve son erreur avant de renoncer à la référence de volume. La validité du pointeur C++ n'est plus utilisée comme preuve d'un fichier ouvert.
- Les destructions des enveloppes invalide/réelle et les gardes de référence sont ordonnées ; aucun double décrément nouveau n'a été identifié dans `open`, `openNextFile` et les fermetures partagées.
- La récupération obtient son bail exclusif avant de consulter `Audio.storageIdle`. Un fichier audio brut `SD` ou un enregistrement actif refuse donc la récupération avant `SD.end`. Le worker audio ne peut pas entrer simultanément dans sa section de stockage. Les données ne sont pas sauvegardées ou fermées de force par cette fonction.
- Les handles cartographiques libérés par `releaseStorageHandles` sont ceux du lecteur hors ligne appartenant à l'UI. Le contrôle final des références a lieu après cette clôture, avant le démontage. Il ne remplace pas une attente arbitraire.
- `probeSdWrite` conserve séparément les erreurs d'écriture, synchronisation, taille, fermeture, lecture et nettoyage. Son fichier est choisi sous le bail exclusif avec une vérification d'absence explicite. Un échec ne réinitialise ni les réglages ni les clés.
- Les lieux ne deviennent sauvegardables qu'après une lecture/validation achevée. Un échec garde l'ancienne table mémoire et `_placesLoaded=false`. Les opérations d'ajout, renommage et suppression refusent une sauvegarde sur cet état incomplet et restaurent la table après publication échouée.

## Défaut repéré pendant la relecture, corrigé avant validation finale

La première proposition de backport conservait systématiquement `fopen_errno` après échec des compatibilités `stat`/`opendir`. Pour un premier `ENOENT` suivi d'un `EIO`, `ENOMEM` ou `ENODEV`, cette priorité aurait fabriqué une absence ; `VFSImpl::exists` emploie aussi ce constructeur, donc `tab5AtomicProbe` aurait pu accepter un fichier comme manquant. Une seconde variante devait également traiter les répertoires confirmés présents par `stat`.

La proposition revue conserve maintenant les erreurs non liées à une absence : première erreur `fopen` hors `ENOENT`/`EISDIR` attendu, puis erreur `stat`, puis erreur `opendir`. Un chemin confirmé présent avant un échec de type absence devient `EAGAIN`, pas une permission de remplacer des données. `ENOENT` n'est retenu que pour des échecs de recherche cohérents avec une absence. Le cas normal `fopen(EISDIR)` suivi d'un échec réel d'`opendir` conserve la cause de ce dernier. Les scénarios supplémentaires ont été transmis au propriétaire du test SDK pour exécution avant gel final.

Cette correction du backport n'est pas attribuée à la 0.8.44 livrée : elle a été identifiée et corrigée au cours de la préparation de la 0.8.45. Les anciens BIN ne sont pas modifiés rétrospectivement.

## Limites

La synchronisation des opérations directes d'un ELF non coopératif, les coupures d'alimentation FAT et le comportement physique du bus restent hors de la preuve fournie par cette relecture. Les accès SDK gardent les mêmes opérations et le même ordre d'E/S ; le backport concerne les causes d'erreur, pas une nouvelle queue ou une nouvelle politique de montage. Aucun nouveau défaut de propriété ou de suppression de données n'a été trouvé dans les chemins examinés après correction de la priorité d'erreurs.
