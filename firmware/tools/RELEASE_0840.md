# Construire et vérifier la livraison 0.8.40

La référence est le ZIP livré `Tab5OS_0.8.39_Evolutions.zip`, SHA-256
`a4274b8fb44642306dc9c78e212d58d9064147d9bbdb0e624b4afb612af7d5ff`.
`verification/baseline-0839-source.json` en fige les sources et les rapports
historiques. Ne pas réétiqueter un ancien rapport pour satisfaire un contrôle .40.

1. Restaurer l’environnement avec `build_linux/prepare_linux.py` et les verrous
   livrés. Exécuter `build_linux/verify_downloads.py`. Les versions sont Arduino
   M5Stack 3.3.9, GCC RISC-V 14.2.0, LVGL 9.6.0, M5Unified 0.2.23 et M5GFX 0.2.30.
2. Stabiliser le code, les ressources et `localization/evolutions-delta-0840.json`.
   Appliquer les traductions avec `localization/apply_evolutions_0840.py --baseline
   <ZIP>`. Les anciennes valeurs et métadonnées sont comparées à la référence.
3. Construire `sdcard/Factory/Tab5OS-0.8.40` avec `rebuild_factory.py <sdcard>
   --version 0.8.40`, puis les en-têtes avec `generate_locale_fallback.py <sketch>`
   et `generate_system_resources.py <sketch>`. Le gazetteer GeoNames est une
   ressource SD/Factory ; son contenu n’est jamais embarqué dans le firmware.
4. Exécuter les tests courants figurant dans `release_scope_0840.json`, l’audit
   `audit_languages.py`, `localization/audit_evolutions_0840.py --baseline <ZIP>`
   et `check_release_resources_0840.py`. Chaque rapport comporte les empreintes
   des sources testées et des preuves produites. Finaliser le registre de
   changements puis exécuter `check_preserved_0840.py`.
5. Geler tous les fichiers du sketch avant `build_linux/compile_linux.py` et
   `build_linux/verify_linux.py`. Le manifeste ignore uniquement `sdcard` et
   `tools/native-apps/image-editor/test-output/` (rendus hôtes non distribués).
   Les autres fichiers sont comparés avant, après et au moment de la livraison.
   Les partitions ne doivent pas être agrandies pour masquer un dépassement.
6. Exécuter `prepare_delivery_0840.py --environment <environnement>`, puis
   `audit_publication.py` et `package_release_0840.py <ZIP situé hors du projet>`.
   L’archive est contrôlée par CRC, réextraite puis soumise aux mêmes contrôles.

Les scripts ne publient rien et ne flashent pas la tablette. La réussite des
contrôles hôtes ne constitue pas une mesure de temps de démarrage, de radio,
de débit USB ou d’absence de scintillement sur le matériel.
