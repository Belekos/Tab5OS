# Essais sur Tab5 — version 0.8.42

Aucun essai physique n’a été effectué dans l’environnement de développement. Ces scénarios complètent les tests hôte et la compilation P4. Conserver une sauvegarde de la SD et employer une copie de travail pour les essais de panne ; aucun formatage ni changement de partitions n’est nécessaire.

## Relevé commun

Noter la révision de la tablette/écran, la version P4/C6 affichée, la SD et les options actives. Après démarrage, attendre l’accueil puis relever une seule fois Informations système, `m` sur la liaison de diagnostic et `tasks` dans le terminal. Les chiffres utiles sont total/libre/minimum/plus grand bloc pour HP, SPM/TCM, LP, PSRAM, et les marges de pile. INTERNAL est un agrégat : ne pas l’additionner à HP/TCM/LP. Le minimum depuis démarrage ne remonte pas après fermeture.

Activer **Réglages → Système → Mesurer les latences** ou `p` sur la liaison de diagnostic. `d` donne maximum de boucle, intervalle entre boucles, phases lentes, événements récents et contexte disponible du reset précédent. Relever puis désactiver/réactiver pour remettre les maxima à zéro avant un autre scénario. `d`, `m` et `tasks` coûtent eux-mêmes du temps : ne pas les imprimer en continu pendant la mesure. Ce sont des durées écoulées comprenant attente/préemption, pas une mesure de charge CPU.

## Scénarios courts

1. **Accueil et flashs** : laisser l’accueil deux minutes sans activité, puis déplacer la souris pendant une minute. Relever `d`, les compteurs DSI, grandes invalidations/images et mémoire. Noter l’heure approximative de tout flash turquoise. Refaire avec Wi-Fi/mesh désactivés puis activés selon les appareils disponibles. Ne pas attribuer un flash à LVGL ou au matériel sur le seul aspect visuel.
2. **Cycles d’applications** : ouvrir/fermer vingt fois Cartes hors ligne, Dessin, un script Python court et un ELF fourni. Attendre la fin effective des workers, puis relever mémoire/piles après le premier et le vingtième cycle. Une baisse durable du libre ou du plus grand bloc doit être corrélée aux tâches restant actives ; des minima bas isolés ne prouvent pas une fuite.
3. **ELF** : lancer un ELF fourni, annuler pendant chargement, revenir à l’accueil puis relancer. La souris doit rester utilisable ; une erreur doit apparaître une seule fois. Ne pas employer un binaire inconnu : le chargeur ne fournit pas d’isolation matérielle.
4. **Radio et vidéo** : radio TLS puis vidéo locale/réseau, changement rapide de source, fermeture pendant connexion et retour audio. L’interface doit répondre pendant la fermeture ; aucune ancienne image ne doit réapparaître. Relever phases lentes et mémoire. Répéter avec Wi-Fi indisponible pour vérifier l’erreur propre et l’usage hors ligne.
5. **Fichiers et paramètres** : sur la copie de SD, enregistrer, copier, renommer, déplacer puis relire un document de contrôle. Modifier une préférence, redémarrer et vérifier sa conservation. Vérifier que l’autosauvegarde garde son choix antérieur. Les scénarios EIO/corruption sont déjà injectés sur hôte ; ne pas arracher la carte de travail pendant une écriture pour les reproduire.
6. **Coffre** : déverrouiller puis verrouiller avec la configuration existante, vérifier un secret de test, fermer/réouvrir. La souris doit répondre pendant le déverrouillage asynchrone. Une erreur SD doit préserver les données et les clés ; ne pas déclencher de récupération ou réinitialisation sur un coffre réel pour tester. Aucun secret/PIN/mot de passe ne doit être joint aux journaux.
7. **Ressources** : sur SD de test, conserver un support officiel modifié et un fichier vide, enlever un autre petit fichier de support. Lancer « Restaurer les fichiers manquants » : seul le fichier absent doit revenir, à l’identique. Annuler puis recommencer ; mémoire restituée et aucun temporaire abandonné après une erreur gérée.
8. **Périphériques disponibles** : connecter Meshtastic et imprimante USB lorsqu’ils sont disponibles ; activer/désactiver/reconnecter, tester hors ligne. En cas de refus Bluetooth, fournir le motif d’admission, les blocs internes et les tâches ; beaucoup de PSRAM libre ne satisfait pas un besoin de pile ou DMA interne.

## Journaux à transmettre en cas d’échec

Version exacte et SHA du BIN, scénario et étapes, options actives, cause de reset, sortie `d` et `m`, liste `tasks`, compteurs écran/SD/réseau et éventuellement photo/vidéo du défaut. Masquer chemins personnels, SSID, adresses et contenus sensibles. Le journal RTC peut survivre à certains resets ; une coupure électrique/brownout ou une écriture RTC interrompue peut le perdre. Il ne remplace pas la liaison série pendant une panne reproductible.
