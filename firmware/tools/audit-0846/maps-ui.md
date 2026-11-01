# Intégration UI Cartes / Média — 0.8.46

Les essais exécutent les fonctions exactes extraites du main livré 0.8.45 puis du main 0.8.46 : `mapNetworkConnected`, `mapDownloadsAllowed`, `mapImportStartEvent`, les quatre callbacks d’onglet Média et le bouton Actualiser. Ils exécutent aussi le bloc final de changement réseau de `serviceMapPage`, sans le recopier sous forme d’une politique de test différente.

Les frontières Wi-Fi, snapshot audio, moteur Cartes, rendu et LVGL sont des adaptateurs. Le test vérifie appels, messages, générations et états ; il ne remplace pas un téléchargement, une contention SD ou un affichage physique. Les enums et le snapshot de routage viennent aussi du véritable en-tête audio. La référence est vérifiée par l’empreinte SHA-256 de l’archive 0.8.45 publiée.

| Scénario | 0.8.45 | 0.8.46 |
|---|---:|---:|
| Assertions exécutées | 5 254 | 5 256 |
| Combinaisons mode/pause/lecture/Wi-Fi/snapshot/SD | 512 | 512 |
| Message Wi-Fi trompeur alors que réseau connecté, ressource occupée | 24 | 0 |
| Imports permis pendant pause d’un fichier local | 0 | 2 |
| Reconstruction de liste pour 4 × 1 000 clics sur l’onglet déjà actif | 4 000 | 0 |
| Relance au passage démarrage radio → flux établi, radio restant en lecture | 0 | 1 |

Les mêmes onglets restent navigables et le bouton Actualiser conserve son rechargement explicite. Les imports refusent toujours carte absente, réseau désactivé/non connecté, snapshot indisponible, enregistrement et lecture locale active. Les limites de plan (0, 1, 50 000, 50 001), autorisation de la source et échec d’enregistrement du réglage sont également vérifiés. La génération avance une fois au démarrage accepté et ne change pas sur refus.

Le snapshot audio comporte volontairement un état Wi-Fi périmé/opposé dans la matrice : la connexion réelle utilisée par le main provient du cache SDK. Les transitions connexion et fin de démarrage radio ne déclenchent qu’une relance ; 1 000 tours sans changement n’en ajoutent pas. Fenêtre d’import et page masquée inhibent le rendu.

Le helper UI 0.8.45 autorisait déjà la radio en pause. Ces essais **ne prouvent pas la disparition du symptôme physique avec radio en pause**. L’audit moteur radio examine séparément la libération de la lease. Il ne faut pas présenter l’exclusion réseau de la radio active comme l’explication de tous les cas en pause.

ASan/UBSan : réussis. LeakSanitizer ne peut pas inspecter `/proc` dans cet environnement et reste désactivé ; aucun résultat de fuite mémoire matérielle n’est revendiqué. Aucun firmware n’a été exécuté sur Tab5 pour cet essai.

Reproduction depuis la racine du projet :

```sh
python3 tools/maps-ui-tests-0846/run.py --baseline-zip /chemin/Tab5OS_0.8.45_SD_Radio.zip --build /tmp/tab5-maps-ui-0846
```

Sources extraites, logs de compilation/exécution et empreintes : `tools/verification/maps-ui-0846/` et `tools/verification/maps-ui-0846.json`. Le rapport doit être régénéré si le main ou l’en-tête audio change, même lorsque le changement est dans une autre zone du fichier.
