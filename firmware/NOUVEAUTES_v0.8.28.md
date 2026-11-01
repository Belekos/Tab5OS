# Tab5OS 0.8.28 — stabilité, commandes et batterie

Cette livraison reprend les redémarrages signalés à l’ouverture ou à la réinitialisation du coffre et au clonage Git. Elle simplifie aussi les commandes série de l’éditeur et ajoute une surveillance de décharge du pack batterie du Tab5.

<!-- build-status -->
**Compilation complète et contrôles de l’image 0.8.28 réussis.** Preuve : [firmware-verification.json](tools/verification/firmware-verification.json).
<!-- /build-status -->

## Éditeur et série

**Affichage → Série** ouvre ou replie le volet. Le bouton dédié est retiré de la barre. **Effacer** vide la réception ; **Annuler** arrête l’envoi de l’éditeur. Les commandes indisponibles restent grisées. La largeur des boutons suit leur libellé et le volet conserve une disposition compatible avec le clavier tactile.

**Envoyer** conserve son fonctionnement : la sélection, ou tout le document sans sélection. Replier le volet conserve l’écoute ; quitter l’éditeur termine sa session. Les limites de tampon et les commandes du terminal sont détaillées dans [SERIAL_0828.md](SERIAL_0828.md).

## Coffre et Git

Un défaut concret du module de verrous du SDK permettait à ECDSA de perturber une opération SHA/AES concurrente. Le correctif amont est repris pour le SDK ESP32-P4/ESP-IDF 5.5.4 utilisé. Les essais reproduisent l’absence d’exclusion avec le SDK initial, puis son rétablissement avec le module corrigé. Cela ne prouve pas que tous les resets signalés avaient cette cause. Le firmware final est également contrôlé pour vérifier qu’il emploie bien le module corrigé.

Le coffre ajoute des phases de diagnostic précises, des tampons PSRAM alignés et des commandes désactivées pendant un travail incompatible. **Copier** remplace le libellé avec durée. Git vérifie les ressources disponibles avant de démarrer et enregistre ses étapes dans le contexte RTC, sans URL ni identifiants. Ses commandes de préparation, commit et diff suivent les données réellement disponibles. Les opérations locales, les transferts HTTPS et les parcours d’erreur sont rejoués.

Voir [COFFRE_0828.md](COFFRE_0828.md), [GIT_0828.md](GIT_0828.md) et [CRYPTO_DIAGNOSTICS_0828.md](CRYPTO_DIAGNOSTICS_0828.md). Si le redémarrage persiste, **Informations système → Avant le reset (contexte)** fournit la dernière phase observée.

## Batterie

La surveillance utilise des mesures valides de tension et de courant, indépendamment du pourcentage estimé. Elle avertit en batterie faible et lance un décompte avant arrêt lors d’une tension durablement critique. Une charge détectée ou des mesures invalides annulent ce décompte ; un faux zéro de capteur ne déclenche donc pas l’arrêt.

L’éditeur tente une copie de secours du document courant en préservant l’original. Le résultat est indiqué ; une carte absente, occupée ou défaillante peut empêcher la copie. L’installation d’un firmware et son redémarrage sont bloqués pendant un état batterie critique connu.

Le Tab5 doit être allumé et initialisé pour charger. Cette surveillance vise à éviter une décharge profonde ; elle ne remplace pas les protections du pack et ne garantit pas la récupération d’une batterie déjà en protection. [BATTERIE_0828.md](BATTERIE_0828.md) explique les seuils et reprend la procédure du fabricant.

## Écran et firmware C6

Les informations système indiquent la variante écran/tactile identifiée, la révision P4 et les versions ESP-Hosted hôte/C6 disponibles. La version C6 est lue lors de l’activation du Wi-Fi, puis consultée depuis un cache. Une majeure connue incompatible bloque une nouvelle recherche ou connexion ; une version inconnue reste signalée comme telle. Aucun flash automatique du C6 ni pilote écran forcé n’est ajouté.

La compilation fournie cible les révisions P4 **0.1 à 1.99**. Voir [COMPATIBILITE_TAB5_0.8.28.md](COMPATIBILITE_TAB5_0.8.28.md) pour les variantes d’écran et les limites de cette vérification logicielle.

## Traductions et vérification

Les libellés et l’aide sont actualisés dans les quatorze langues. Les rapports de la 0.8.27 gardent leurs dates et leurs empreintes. Un résultat antérieur n’est repris que si les sources et le banc de test contrôlés correspondent encore exactement ; les parcours modifiés sont rejoués.

<!-- resource-summary -->
Les 14 packs couvrent les 3178 clés de référence contrôlées. Factory contient 174 ressources officielles ; le secours firmware en contient 40. Les grandes polices et les dictionnaires de saisie sont distribués sur SD. Preuves : [languages-0828.json](tools/verification/languages-0828.json), [ime-0828.json](tools/verification/ime-0828.json), [resources-0828.json](tools/verification/resources-0828.json).
<!-- /resource-summary -->

Aucun essai sur un Tab5 physique ni téléversement n’est revendiqué. Les tests sur ordinateur ne prouvent pas à eux seuls la disparition d’un redémarrage matériel.
