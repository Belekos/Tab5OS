# Revue indépendante — identité et publication des médias

Cette revue porte sur le code final du modèle Média, ses callbacks UI, les catalogues audio/vidéo et les commandes différées. Elle a été menée par un autre agent que l'auteur du changement. Elle ne constitue pas un essai matériel du lecteur ou de la carte SD.

## Défauts trouvés puis corrigés pendant la revue

La transaction d'affichage conservait initialement les anciens indices après un échec d'allocation. Or un rechargement du catalogue pouvait avoir réaffecté ces indices : une ancienne ligne d'enregistrement pouvait désigner un autre fichier. L'ancien modèle n'est maintenant conservé que si sa révision reste valide. Sinon ses références sont libérées, les actions sont refusées et l'échec reste visible. Une allocation refusée ne provoque pas une nouvelle reconstruction à chaque passage UI.

La copie de l'identité à mémoriser ne suffisait pas à protéger le lancement vidéo, qui relisait ensuite le même indice. La seconde copie est désormais suivie d'une vérification de la révision attendue ; le lecteur reçoit cette copie stable. Les commandes audio gardent également leur révision jusqu'au traitement réel par le worker, y compris après report par une activité prioritaire.

Les reprises de radio et les déplacements dans les listes utilisaient encore certains indices conservés avant un rechargement. La radio retrouve sa sélection par URL lors de la reprise, après attente Wi-Fi, après pause et après activité prioritaire. Une station supprimée n'est pas remplacée silencieusement par celle qui occupe son ancien indice. Le passage au titre local suivant retrouve aussi le chemin courant avant de choisir son voisin. Pour la radio, le choix du voisin capture une révision vérifiée lors de son exécution.

## Propriété et concurrence vérifiées

- Les mutations de chaque vecteur sont protégées par leur verrou existant ou par l'exclusion de récupération du stockage. Chaque mutation publie une révision impaire pendant la modification, paire après. Aucun emboîtement de deux gardes portant sur la même révision n'a été trouvé dans les appels actuels.
- Les révisions pistes/enregistrements, radios et vidéos sont distinctes. Une reconstruction des pistes ne périme pas les radios affichées. Les enregistrements partagent volontairement le catalogue des pistes.
- Une lecture de modèle comportant plusieurs copies peut croiser une mutation, mais sa publication est rejetée si la révision initiale ne correspond plus. Le contrôle après copie protège également les favoris et le chemin transmis à la confirmation de suppression.
- Les deux tableaux de références appartiennent au modèle ; la publication échange leurs rôles. L'échec du tri ou de l'agrandissement conserve les allocations précédentes. L'arène des noms utilise des offsets jusqu'à la fin de ses réallocations, puis des pointeurs stables pendant le tri. Destruction et récupération libèrent les deux tableaux.
- Les callbacks de clic copient leur entrée avant l'appel susceptible de reconstruire le modèle. Un appui commencé avant une reconstruction ne peut pas activer sa nouvelle ligne. Aucun thread ni timer supplémentaire n'est introduit par cette transaction.
- Après une erreur de tri, la préférence globale demandée est conservée ; le sélecteur montre l'ordre de la liste encore affichée. Une nouvelle action explicite peut réessayer la préférence demandée. Ce comportement est intentionnel et n'est pas présenté comme une application réussie du tri refusé.

## Preuves et limites

`media-ui-0847.json` couvre le vrai modèle, les fonctions UI extraites sans réécriture et LVGL réel : conservation du modèle sur erreur, révisions périmées, suppression, mutation pendant la copie, seconde copie vidéo et absence de boucle de reconstruction après OOM. `audio-catalogue-0847.json` couvre les vrais handlers et les fonctions d'ouverture/reprise, avec frontières réseau, codecs, files natives et fichiers contrôlées. Les tests hôte utilisent ASan et UBSan ; ils n'établissent pas l'absence de flash physique.

Aucun nouveau défaut bloquant de propriété ou d'identité n'a été trouvé dans le périmètre final revu. L'énumération initiale des fichiers et certaines métadonnées restent synchrones dans l'UI. Les groupes de navigation et plusieurs catalogues emploient encore des conteneurs dont une allocation échouée n'est pas uniformément récupérable sur le firmware sans exceptions. Ces dettes ne sont pas corrigées par la seule transaction d'affichage et restent à traiter avant une qualification générale de production.
