# Tab5OS 0.8.15 — stabilité et fonctionnement autonome

> Note historique. La version courante est la 0.8.16 : Mail, Internet, Quake et Duke Nukem 3D sont retirés de la distribution principale. Lire NOUVEAUTES_0.8.16.md et VERIFICATION.md pour son périmètre et ses validations.

Cette version reprend les sources 0.8.14 et leur comparaison avec 0.8.13. Priorité : travailler, gérer ses fichiers et ses comptes depuis le Tab5, avec des opérations qui restent réactives et des limites explicites.

## Corrections

- HTML : les images sont préparées hors du dessin LVGL, leurs descripteurs restent stables ; récupération après allocation refusée. Limites appliquées aux éléments réellement produits, aux expressions CSS, aux imports et aux téléchargements. Le navigateur reste sans JavaScript, cookies ni formulaires POST.
- Applications natives : les appels de temps, entrées, dessin et fichiers laissent périodiquement le processeur aux autres tâches ; arène alignée et budget de pile contrôlé. Le watchdog reste actif. Les jeux ne sont pas développés davantage dans ce correctif.
- Accueil : barre de défilement visible ; défilement de la liste Python également explicite.
- Explorateur : bouton racine SD, navigation et stockage regroupés ; copier/couper/coller côte à côte ; recherche identifiée et bouton « Cachés » explicite. « Enregistrer sous » retiré de l’explorateur. « Ouvrir avec » reste une action du fichier sélectionné.
- Explorateur : listes chargées par petites étapes, pages de 128 éléments. Aperçu texte de 8 Kio, sans comptage intégral du document ; aucun index EPUB construit à la sélection. Aperçus de dossiers bornés.
- Copier : tâche séparée, progression, annulation, refus d’écrasement, contrôle des écritures et relecture de la copie. Original conservé en cas d’échec. Un fichier ou dossier partiel peut rester à destination et est signalé.
- Couper : renommage direct sur un même support lorsque possible. Le repli par copie, notamment entre SD et USB, conserve l’original et l’annonce : il ne prétend pas avoir supprimé la source.
- Coffre : mode natif propre à l’appareil, choisi explicitement, puis ouverture sans saisie. Ancien coffre conservé ; migration avec son ancien mot de passe. Le mode appareil protège surtout la SD retirée, pas l’accès à un Tab5 déverrouillé. Pas de PIN dans cette livraison.
- Mail : compte original et quatre profils distincts ; choisir ou créer un autre compte reste possible depuis l’écran verrouillé.
- MicroPython : catalogue de bibliothèques isolé, filtres par carte prévue et catégorie, import de modules/dossiers et paquets mip, création de projets et ajout dans leur dossier lib. Les modules importés ne sont pas exécutés pour être catalogués. La gestion des modules propres au Tab5 reste séparée.
- MicroPython : reprise des temporaires de téléchargement corrigée. Les fichiers .mpy sont conservés avec une compatibilité non vérifiée ; aucun nouveau téléverseur universel n’est annoncé.

## Limites assumées

Le rendu, la fluidité, les resets réels et les périphériques n’ont pas été essayés sur un Tab5 physique dans cette session. Les tests hôtes et la compilation vérifient des défauts précis, sans garantir que toutes les causes des plantages ont disparu.

Les ELF natifs partagent l’espace mémoire du firmware : une erreur de pointeur ou une boucle qui n’appelle aucun service de l’OS peut encore provoquer un reset. Le P4 ne fournit pas ici d’isolation comparable à celle d’un système de bureau. Doom, Quake et Duke3D restent à valider sur matériel.

L’ancien déverrouillage par mot de passe et l’export protégé des clés conservent leur dérivation coûteuse ; la première migration peut donc prendre du temps. Un mot de passe oublié ne peut pas être reconstruit.

Le catalogue MicroPython permet de préparer des projets pour d’autres cartes. Il ne certifie ni les broches ni les API de ces cartes, et n’ajoute pas à lui seul leur connexion USB, leur firmware ou leur téléverseur.

Voir VERIFICATION.md pour les contrôles réellement exécutés, SECURITE.md pour la migration et MICROPYTHON_BIBLIOTHEQUES.md pour le catalogue.
