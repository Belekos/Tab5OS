# Ressources et mesures — Tab5OS 0.8.45

Comparaison avec le binaire livré 0.8.44. Même SDK M5Stack Arduino 3.3.9 / ESP-IDF 5.5.4 / GCC RISC-V 14.2, M5Unified 0.2.23, M5GFX 0.2.30, LVGL 9.6.0. La copie FS reçoit le backport d'erreurs décrit dans l'audit. Aucun tas libre, CPU, FPS, débit SD ou résultat acoustique n'a été mesuré sur tablette.

## Binaire lié

| Mesure | 0.8.44 | 0.8.45 | Évolution |
| --- | ---: | ---: | ---: |
| Firmware OTA, octets | 6 065 728 | 6 067 488 | +1 760 |
| Partition OTA, octets | 6 553 600 | 6 553 600 | 0 |
| Marge OTA, octets | 487 872 | 486 112 | −1 760 |
| Occupation | 92,56 % | 92,58 % | ≈ +0,03 point |
| Données internes statiques + BSS, octets | 93 268 | 93 268 | 0 |
| `.dram0.data`, octets | 31 364 | 31 364 | 0 |
| `.dram0.bss`, octets | 45 220 | 45 220 | 0 |
| `.dram1.bss`, octets | 16 684 | 16 684 | 0 |
| `.iram0.text`, octets | 98 494 | 98 494 | 0 |
| `.tcm.text` / `.tcm.data`, octets | 794 / 64 | 794 / 64 | 0 |
| `.rtc_noinit`, octets | 1 200 | 1 200 | 0 |

SHA-256 du BIN : `0b732239e9f63673a0d6afd67339206b8891f2f699e1e0dc9c26f7e0b940afbc`.

La table de partitions est inchangée. Les objectifs de cette correction sont la propriété des ressources et l'intégrité des erreurs/données ; aucun gain de SRAM au démarrage n'est revendiqué. Le compte des fichiers ouverts est corrigé, pas artificiellement remis à zéro.

## Résultats des scénarios contrôlés

| Scénario hôte sur code réel | Référence | Correction |
| --- | ---: | ---: |
| Fichier invalide persistant après ouverture ratée : références SD | 1 | 0 |
| Contrôleur de reconnexion avec ce fichier : atteint le démontage | non | oui |
| Faux succès du probe dans 12 erreurs injectées de sync/fermeture | 12 | 0 |
| Reprises de bus après deux erreurs non transport dans le test | 2 | 0 |
| Reprises pour les quatre erreurs transport reconnues | 4 | 4 |
| Erreur EBUSY d'un autre worker supprime la reprise EIO du probe | oui, avant résultat local | non |
| Redémarrage I2S supplémentaire à la première fréquence MP3 | 1 | 0 |
| Redémarrages I2S d'une séquence de fréquences source | 9 cumulés | 0 |
| Anciennes tuiles/tampons possédés par l'UI libérés à tort au zoom | aucun dans le modèle | aucun |

La référence du test de course « EBUSY » est l'état intermédiaire avant isolation du résultat local, conservé avec son empreinte. Les autres références sont la 0.8.44 officielle. Les résultats ne sont pas des mesures de récupération physique, de temps réel ou de suppression du claquement acoustique.

Les tests FS/VFS réels couvrent 1 000 cycles et 400 accès concurrents, avec zéro référence restante. La radio complète passe 100 cycles producteur/consommateur/fermeture avec zéro client ou tampon suivi restant. Le pilote PCM passe 100 cycles entre clients. Les recherches et générations libèrent les allocations suivies, y compris après remplacement et annulation. LSan n'est pas exploitable ici ; ces contrôles explicites et ASan/UBSan ne remplacent pas un cycle complet de l'OS physique.

## Coûts et limites

Aucune pile de tâche, marge d'admission, taille DMA ou capacité d'application n'est abaissée. Aucune nouvelle tâche ou boucle de diagnostic permanente n'est ajoutée. La configuration d'écran est lue seulement sur `d` ; son buffer de formatage utilise une pile ponctuelle, sans état permanent supplémentaire.

La sortie I2S garde l'horloge configurée et utilise le rééchantillonnage M5Unified. Les fréquences source sont conservées par tampon, mais le coût CPU et la qualité sonore doivent être mesurés sur l'appareil. Supprimer les redémarrages évite des réallocations et séquences d'alimentation superflues ; cela ne prouve pas l'absence de toute cause acoustique.

Le diagnostic matériel doit toujours distinguer interne libre/minimum/plus grand bloc, DMA et PSRAM. Les 32 Mio de PSRAM ne remplacent pas toutes les piles ou allocations internes contiguës. Les sous-régions TCM/SPM/LP ne s'ajoutent pas arbitrairement au total INTERNAL du SDK. Le C6 a sa mémoire et son firmware de communications ; il ne fournit pas un tas supplémentaire au P4.

Les copies et rendus PSRAM peuvent concurrencer le scan DSI. Les tests de cache/rotation/ownership n'ont pas de modèle fidèle du débit du bus et ne permettent donc pas de quantifier les flashs. Les compteurs `d` sur la tablette restent nécessaires.

## Compilation et avertissements

La compilation Arduino ordinaire supprime les avertissements avec `-w`. Un passage distinct sur neuf unités critiques retire cette suppression et active les contrôles supplémentaires, sans changer les options du firmware livré. Il conserve les avertissements existants : notamment dépréciations d'API et initialisations/variables/indentations anciennes. Le succès n'est pas présenté comme un code sans avertissement. La comparaison des mêmes neuf unités avec les sources authentifiées 0.8.44 retrouve 343 avertissements avant/après et aucun delta par catégorie ; cela ne prouve pas l'absence de défaut d'exécution.

La vérification du firmware lie les mesures aux sources exactes, au FS corrigé réellement compilé, à ses objets et aux méthodes VFS liées. Voir `tools/verification/firmware-verification-0845.json`, le rapport des avertissements et [VERIFICATION_0.8.45.md](VERIFICATION_0.8.45.md).
