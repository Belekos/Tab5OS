# Construction et contrôle de la version 0.8.49

État initial : archive livrée `Tab5OS_0.8.48_Affichage_Audio_Video.zip`, SHA256
`599483c491c253a4e41c813b2e94aecf30e4d3c85d3459486dc637e1abac095b`.
Le manifeste `tools/verification/baseline-0848-source.json` a été calculé sur
les octets de cette archive. Les rapports historiques restent inchangés.

La compilation se fait dans un répertoire distinct `build_0849`, hors du
projet distribué. Les archives et installations immuables de la chaîne .48
sont réutilisées, après vérification de leurs empreintes. Les copies de
bibliothèques modifiées sont régénérées dans l'environnement .49. Ni les
bibliothèques installées ni le SDK d'origine ne sont modifiés.

La chaîne est Arduino CLI 1.5.1, M5Stack Arduino 3.3.9 / ESP-IDF 5.5.4,
GCC RISC-V 14.2.0, M5Unified 0.2.23, M5GFX 0.2.30 et LVGL 9.6.0.
Le profil matériel conservé est `ChipVariant=prev3`, PSRAM activée,
partitionnement `default`, flash DIO. La compilation ne détecte pas une
révision physique de puce à distance.

Les scripts Linux et Windows ajoutent `--wrap=esp_cache_msync` au lien.
`verify_cache_sync.py` examine le binaire ELF final : emplacement IRAM du
wrapper et du point d'entrée natif, appel du wrapper par le décodeur JPEG et
appel de l'implémentation native par le wrapper. L'ajout d'une option au
compilateur n'est pas considéré comme une preuve de son utilisation.

La compilation définitive doit commencer après stabilisation des sources.
Les manifestes avant/après, les copies Arduino, l'image ESP32-P4, l'ELF,
les dépendances réellement compilées et les partitions sont vérifiés.
Une archive finale est testée par CRC puis extraite et revalidée. La
préparation des scripts seule n'établit ni une compilation réussie, ni une
correction des symptômes physiques.

Aucun flash de tablette, essai audio physique, mesure de latence DSI,
consommation de heap au repos ou mesure de CPU n'est effectué dans cet
environnement. Les chiffres de sections du lien ne remplacent pas les
mesures des tas SRAM/PSRAM en exécution.
