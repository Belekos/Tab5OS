# Audio — correction 0.8.48

## Défaut confirmé et correction

La copie M5Unified 0.2.23 utilisée par 0.8.47 effaçait les requêtes audio dans
`Speaker_Class::_end_locked()` mais conservait `channel_info_t.diff`, `index`
et les deux paires d'échantillons d'interpolation `liner_buf`. Après un arrêt
avant la fin d'un bloc source, `spk_task` pouvait reprendre directement à
`label_continue_sample` avec une phase négative et interpoler les échantillons
de la lecture précédente avant de lire le premier échantillon du nouveau flux.
Ce défaut ne dépend pas du volume de données restant en PSRAM.

Le correctif isolé `tools/build_linux/prepare_m5unified_audio.py` remet uniquement
ces valeurs à zéro **après la fin du worker**, dans la boucle existante de
nettoyage. Les volumes, la configuration, le traitement continu et la pause ne
sont pas modifiés. Aucun nouveau tampon, délai, fondu ou worker n'est introduit.
Le SDK installé reste intact; ses trois sources d'entrée sont vérifiées par
SHA-256 avant la préparation de la copie compilée.

## Vérifications exécutées lors de cette reprise

Les tests extraient la totalité de `spk_task`, `_end_locked`, `wav_info_t::clear`
et les véritables types du mélangeur. Seuls l'I2S et l'ordonnancement de la tâche
sont des frontières contrôlées. Le programme de comparaison 0.8.47 vérifie le
SHA-256 de son `Speaker_Class.inl` effectivement patché.

- 192 cycles son constant → arrêt → PCM nul, huit fréquences d'entrée, trois
  longueurs de bloc, mono/stéréo et quatre points d'arrêt : 192 contaminations
  avant, aucune après. Pic de contamination du test : 3398 → 0 unités PCM.
- 16 384 échantillons d'une lecture continue : fichier PCM strictement identique.
- Les huit volumes de canaux sont conservés; démarrage à froid sur PCM nul valide.
- ASan/UBSan : succès. LSan ne peut pas fonctionner dans cet environnement
  (`/proc/.../task` inaccessible); aucune mesure de fuite matérielle n'est déduite.
- Régression démarrage 0.8.47 rejouée avec le correctif 0.8.48 : 4504 vérifications,
  24 cycles et 41 cas d'échec; les 1000 reprises du chemin de repos passent,
  sans redémarrage I2S externe dans ce modèle.

Les nombres ci-dessus proviennent des tests de cette reprise, et ne réutilisent
pas les résultats d'un ancien dossier temporaire. Voir
`tools/verification/audio-reset-0848.json`,
`audio-startup-regression-0848.json` et `audio-sdk-target-0848.json`.

## Limite acoustique

Cette correction élimine la contamination numérique reproduite. Elle ne prouve
pas l'absence d'un claquement analogique au premier démarrage de l'amplificateur,
ni l'absence de sous-alimentation I2S en situation réelle. Sur Tab5, comparer un
WAV de silence démarré après allumage puis après un morceau interrompu, et
conserver les journaux `[AUDIO]` et la trace de redémarrage éventuelle. Aucun
changement des registres codec ou délai arbitraire n'est ajouté sans mesure.

## Reproduction

Depuis le répertoire contenant les archives extraites, avec M5Unified 0.2.23
original installé et les chemins adaptés :

```sh
python3 tools/audio-reset-tests-0848/run.py --sdk /chemin/M5Unified --baseline-helper /chemin/0.8.47/tools/build_linux/prepare_m5unified_audio.py --work /tmp/tab5-audio-reset
python3 tools/audio-reset-tests-0848/startup.py --sdk /chemin/M5Unified --work /tmp/tab5-audio-startup
python3 tools/audio-reset-tests-0848/target_sdk.py --sdk /chemin/M5Unified --database /chemin/compile_commands.json --work /tmp/tab5-audio-target
```
