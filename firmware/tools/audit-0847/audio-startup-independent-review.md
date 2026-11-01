# Seconde revue indépendante du démarrage audio — 0.8.47

Cette revue porte sur la recette `prepare_m5unified_audio.py`, la copie M5Unified 0.2.23 réellement produite, le pilote commun Tab5 et les sources ESP-IDF 5.5.4 du canal I2S. Elle ne conclut pas à la disparition physique du claquement et ne couvre pas une panne électrique du codec.

## Défaut supplémentaire trouvé et corrigé

`Speaker_Class::_begin_raw()` ne vérifiait pas le résultat de `xSemaphoreCreateBinary()`. Le worker appelle ensuite `xSemaphoreGive()` et les producteurs `xSemaphoreTake()` sur ce handle. Une pénurie de mémoire interne pouvait donc aboutir à une assertion malgré un démarrage annoncé réussi. Ce défaut était présent dans la bibliothèque épinglée, avant les modifications de démarrage.

Le propriétaire du correctif a ajouté une sortie `false` immédiatement après l'allocation échouée, avant tout callback matériel. `_begin_lock` est libéré ; `begin()` libère son verrou de requête. L'injection d'échec vérifie l'absence d'accès I²C, d'allocation I2S et d'activation d'horloge, puis une nouvelle tentative réussie. Cette correction traite la cause de l'assertion, sans redémarrage ni deuxième tentative automatique.

## Cycle de vie vérifié dans les sources

| Situation | Vérification |
|---|---|
| Démarrage de l'OS | `M5.begin(cfg)` configure le Speaker et ses callbacks ; `_begin_audio` ne démarre pas son worker. Le pilote Tab5 active l'option avant le premier `claim()`. |
| Premier démarrage | Amplificateur désactivé ; setup I2S ; taille DMA native interrogée ; horloges et zéros ; initialisation codec en sourdine ; démutage ; création du worker ; succès publié en dernier. |
| Taille réelle des DMA | `i2s_channel_get_info()` renvoie `desc_num * buf_size`. `i2s_get_buf_size()` peut aligner la longueur demandée. Le calcul de préchargement n'assimile donc plus la longueur demandée à la longueur réellement allouée. |
| Propriété des zéros | `i2s_channel_write()` copie les octets dans les buffers DMA avant retour. Le tableau temporaire de 256 octets n'est pas conservé par le pilote. Les tailles et additions sont contrôlées. |
| Attente sans PCM, notamment HTTP | Les buffers natifs sont initialement alloués par `calloc` puis synchronisés vers la mémoire. `auto_clear=true` efface chaque buffer terminé avant sa remise en file. La branche codec externe conserve les horloges et attend une notification ; seul le DAC interne utilise la branche arrêt/reprise. |
| Première donnée après attente | L'état local du worker est déjà `spk_i2s_run` en mode différé. Il ne redémarre pas une seconde fois le canal préchargé. |
| Erreur setup, information DMA, activation, écriture ou codec | Aucun succès n'est publié. Le callback de désactivation est tenté, le canal arrêté puis supprimé. Un échec après écriture partielle déclenche la fermeture, pas l'activation de l'amplificateur. |
| Échec création de tâche | `_end_locked()` retrouve `_task_running=true` mais aucun handle de tâche ; il désactive, remet l'état au repos et arrête le canal préchargé avant sa suppression. |
| Arrêt normal | Désactivation codec/amplificateur avant fermeture du worker ; jonction avant effacement ou libération des blocs PCM. Le second `i2s_channel_disable()` sur un canal déjà READY retourne `ESP_ERR_INVALID_STATE` sans le modifier ; `i2s_del_channel()` reste autorisé. |
| Réutilisation des blocs PCM | Les deux slots Speaker sont le son courant et le son en attente ; `current` est une copie de métadonnées du slot en lecture. Trois blocs côté producteur permettent le bloc en écriture, le courant et le suivant. Le basculement intervient après admission ; la fermeture attend le lecteur avant `clear/free`. |

Le code I2S natif vérifie aussi ses propres allocations de canal, de files et de sémaphores. La suppression conserve le handle si elle échoue ; elle ne prétend pas avoir libéré un canal encore actif.

## Ancien repli DMA

La source de `Tab5AudioDriver.cpp` dans l'archive de référence 0.8.46 définit `TAB5_SPEAKER_LONG_DMA` à 0. Le type `speaker_config_t` épinglé utilise 256 trames ×8 descripteurs. Les commandes de compilation et leurs fichiers de réponse ne définissent pas cette option autrement. Le deuxième `begin()` ancien réappliquait donc exactement 256×8. Sa suppression ne retire pas un gain mémoire effectif de la configuration livrée. L'option inactive 512×12 n'est pas présentée comme une configuration testée de cette livraison.

## Vérification exécutée par la seconde revue

Après correction du sémaphore, les corps exacts de `begin`, `_begin_raw`, `end`, `_end_locked`, du callback Tab5 et de l'écriture I²C groupée ont été comparés à ceux incorporés dans le banc de test. Les deux branches réelles attente/reprise du worker ont également été comparées. Les exécutables ASan/UBSan ont ensuite été relancés :

- Démarrage : 4 504 vérifications, 24 configurations, 41 cas d'échec, zéro activation avant horloge.
- Attente/reprise : 20 040 vérifications, 1 000 cycles de codec externe, aucun redémarrage d'horloge ; un contrôle DAC interne exerce effectivement son redémarrage.

Il s'agit d'une seconde revue des sources et d'une réexécution du banc de production extrait, pas d'un nouveau modèle matériel indépendant. I²C, DMA et ordonnanceur sont des frontières contrôlées ; LeakSanitizer n'est pas utilisé. Les empreintes et résultats sont enregistrés dans `tools/verification/audio-independent-review-0847.json`. La source Speaker corrigée porte l'empreinte `c8c4a28ae068c7f953179321a2fe9e4413dfea78803c38ceb4b0429bb29bfa21`.

## Limites restantes

Le worker contient encore des écritures I2S avec `portMAX_DELAY` et sa jonction attend sa disparition avec `vTaskDelay(1)`. Si le périphérique cesse totalement de fournir ses événements DMA, une fermeture peut rester bloquée. Cette attente préexistante assure la propriété mémoire en fonctionnement normal ; la retirer ou libérer les blocs avant jonction serait dangereux. Une récupération après panne DMA complète demande un essai ciblé et une stratégie d'annulation du pilote, sans destruction concurrente de ses ressources.

Les commandes I²C de désactivation peuvent elles-mêmes échouer sur un bus défaillant ; leur tentative ne garantit pas électriquement le mute. Le démarrage refuse néanmoins d'annoncer une réussite après une erreur détectée. Les marges de pile réelles, les fronts MCLK/BCLK, le routage casque et le bruit acoustique restent à vérifier sur Tab5. Aucune mesure de ces phénomènes n'a été effectuée ici.

Références relues : M5Unified 0.2.23 épinglé ; [I2S ESP-IDF 5.5.4](https://github.com/espressif/esp-idf/blob/v5.5.4/components/esp_driver_i2s/i2s_common.c) et [configuration standard I2S](https://github.com/espressif/esp-idf/blob/v5.5.4/components/esp_driver_i2s/i2s_std.c). Le rapport de compilation globale doit confirmer la sélection de la copie isolée et les empreintes finales ; cette revue ne remplace pas ce contrôle.
