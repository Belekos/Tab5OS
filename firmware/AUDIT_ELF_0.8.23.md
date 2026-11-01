# Applications ELF : restitution des ressources en 0.8.23

L'examen du code et les tests sur ordinateur ont identifié une fin de vie audio incorrecte. Ils ne permettent pas d'affirmer que toute la pression mémoire observée sur la tablette vient de ce défaut. Aucune mesure de SRAM ou de PSRAM physique n'a été réalisée pour cet audit.

## Corrections

- **Libération audio réellement terminée.** Le pilote commun peut refuser de prendre son verrou après 250 ms, notamment pendant le contrôle de la sortie casque. Le worker ELF considérait pourtant l'audio libéré, puis autorisait la destruction de son contexte. Il réessaie désormais sur sa propre tâche, en cédant le processeur, jusqu'à ce que le pilote ait rendu la propriété et son tampon PCM. L'interface ne fait pas cette attente. Le buffer PCM fait 12 Kio ; le pilote arrête aussi Speaker/I2S avant de le libérer. Le test reproduit zéro à trois refus successifs et échoue avec le worker de la 0.8.22.
- **Comptage des blocs alignés.** Les allocations ELF sont alignées sur 16 octets. Le quota et la réservation comptent désormais la taille arrondie effectivement demandée au tas, au lieu de la seule taille demandée par l'application.
- **Fin de vie de l'image native.** La fermeture supprime explicitement le cache LVGL associé au descripteur d'image, puis remet le descripteur à zéro avant de libérer les deux tampons vidéo. Cela couvre aussi les variantes de décodage qui conservent une image convertie ou alignée. Le test de ce chemin modélise LVGL ; il ne mesure pas un cache matériel.
- **Diagnostic par cycle.** La sortie série indique l'état mémoire avant un lancement, après la restitution des ressources et lors d'un contrôle différé de 250 ms. Un lancement abandonné est également suivi. Les chiffres `interne`, `PSRAM` et `bloc` sont les valeurs réellement lues sur l'appareil ; le champ `min` est un minimum historique et ne remonte pas après une libération.

## Ressources suivies

Le loader restitue les 32 allocations maximales de l'application, ferme ses huit fichiers, libère l'image relocalisée, détruit le contexte et son bail de stockage, ferme l'écran vidéo et détache le panneau générique ABI 5. Les appels matériels sont libérés après le retour du programme. Les connexions du panneau passent par les outils réseau supervisés de l'OS ; son callback de sortie n'est utilisé que pendant l'appel synchrone.

Pour une pile interne, le contexte de l'application peut être détruit dès le dernier accès du worker, mais FreeRTOS récupère sa pile et son TCB ultérieurement. Le contrôle différé permet d'observer cette évolution ; son délai n'est pas une preuve que la tâche idle s'est exécutée. Pour une pile externe, le loader attend l'état suspendu puis appelle `vTaskDeleteWithCaps` avant de détruire le contexte. Aucun emplacement TLS réservé au SDK n'est réutilisé.

Les deux buffers de console ELF, 8 Kio chacun, restent volontairement disponibles pour consulter la sortie après la fin du programme. Ils sont enregistrés comme cache récupérable et les tests vérifient leur éviction complète, y compris après une allocation partielle. Les caches d'autres applications peuvent aussi modifier la mémoire globale entre deux mesures ; une variation du tas total ne désigne pas à elle seule une fuite de l'ELF.

## Vérifications

| Rapport | Ce qui est exécuté | Limite |
| --- | --- | --- |
| [Cycle worker](tools/verification/elf-lifecycle-0823.json) | Fonctions worker/service de production extraites ; sortie normale, erreur, annulation, audio, arguments, publication de fin et ordre suspension/suppression | Ordonnanceur, audio et entrée ELF simulés ; pas d'exécution RISC-V |
| [Ressources](tools/verification/elf-resources-0823.json) | Fonctions de production d'allocation, libération, cleanup, cache console, fermeture vidéo et deinit ELF ; véritable gestionnaire de réservations ; 1 024 cycles, 1 026 échecs d'allocation injectés | Allocateur suivi sur ordinateur ; périphériques, fichiers et LVGL modélisés |
| [Interface ABI 5](tools/verification/elf-api5-0823.json) | Accesseurs et wrappers de production ; préfixe ABI conservé, appels limités au worker, annulation, absence de commandes de jeu automatiques | Broker et réseau simulés ; les tests du panneau sont complémentaires |
| [Compilation cible](tools/verification/elf-target-stack-0823.json) | Compilation P4 de cinq unités ELF, affichage, broker et outils ; plus grands cadres de pile respectifs : 608, 128, 144, 368 et 1 088 octets | Les cadres individuels ne bornent pas une chaîne complète d'appels ; aucune mesure de pile physique |
| [Application Pentest](tools/verification/app-panel-0823.json) | Source C de l'ELF et broker de production, 1 000 sessions, 10 000 écritures concurrentes avec effacement, annulation et refus mémoire | Réseau simulé ; zéro allocation suivie restante après fermeture |
| [Interface Pentest](tools/verification/app-panel-ui-0823.json) | 625 contrôles avec le vrai LVGL : modes, rapport approchant 160 Kio, sélection stable, défilement, corbeille, Ctrl+L et clic droit | Affichage sur ordinateur, sans tactile physique |
| [Rapport multilingue](tools/verification/pentest-buffer-0823.json) | 176 contrôles du tampon de production, limites UTF-8 et rapports de 1 024 ports dans les 14 langues ; maximum 137 897 octets pour 163 840 disponibles | Rapports construits à partir du format réel des résultats, sans effectuer de scan réseau |
| [Binaire Pentest](tools/verification/pentest-elf-0823.json) | ELF RV32 ilp32 recompilé, accepté par le validateur de production ; import ABI 5 unique, demande de pile de 32 Kio ; import ABI inconnu refusé | Compilation et validation de structure, sans exécution RISC-V sur la tablette |

Les blocs, fichiers et baux suivis par le test de ressources reviennent à zéro. Cette observation porte sur les ressources du harness, pas sur l'ensemble des allocations du SDK de la tablette. Les programmes ELF restent du code de confiance partageant l'espace mémoire de l'OS : un accès hors limites, un programme qui ne coopère plus ou un défaut de pilote ne peut pas être rendu sûr par le seul suivi des allocations.

Pour poursuivre le diagnostic matériel, conserver la sortie série d'une séquence lancement → jeu → sortie → deuxième lancement, avec les lignes `[MEM]` et `[STACK]`. Ne pas comparer seulement le minimum historique du tas ; comparer la mémoire libre, le plus grand bloc et la présence éventuelle d'une libération encore en cours.
