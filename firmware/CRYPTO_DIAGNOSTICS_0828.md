# Chiffrement concurrent et contexte de redémarrage — 0.8.28

Le SDK livré avec Arduino M5Stack 3.3.9 pour cette tablette utilise ESP-IDF 5.5.4, révision `735507283d`, variante ESP32-P4 antérieure à la révision 3. Le déverrouillage et la réinitialisation du coffre ainsi que Git emploient des tâches FreeRTOS ordinaires avec une pile interne. L'audit n'a pas établi de défaut dans leur création ou dans le transfert de réservation mémoire.

## Correctif SDK précis

La version livrée du module `esp_crypto_lock.c` permet une vérification ECDSA pendant une opération SHA/AES : le verrou ECDSA protège ECC et MPI, mais omet SHA/AES. Or le reset matériel ECDSA affecte également SHA, qui partage son DMA avec AES. Espressif a corrigé ce couplage dans [`7fc2aaf`](https://github.com/espressif/esp-idf/commit/7fc2aaf).

`Tab5CryptoLockCompat.c` reprend le module de verrous complet du SDK livré et applique les modifications ECDSA de ce correctif : ordre ECDSA → ECC → SHA/AES → MPI, libération inverse, protection MPI sous sa capacité matérielle même lorsque la révision n'utilise pas son moteur. La partie Key Manager du correctif amont n'est pas présentée comme intégrée ici.

La compilation limite cette adaptation à ESP32-P4 et ESP-IDF 5.5.4. Les autres cibles et versions conservent leur implémentation SDK. Aucun réglage particulier du linker Arduino IDE n'est requis : l'objet du sketch définit les symboles de verrouillage, empêchant l'extraction du module original de l'archive SDK. Le contrôle du firmware final doit confirmer leur provenance et l'absence de doublon.

Source d'origine : [`735507283d/components/esp_security/src/esp_crypto_lock.c`](https://github.com/espressif/esp-idf/blob/735507283d/components/esp_security/src/esp_crypto_lock.c). Empreinte SHA-256 du texte source amont en fins de ligne LF : `5ec2287139419de96af04c40afdff39829655d3b67893f2bdef8096070068c2b`. Licence et copyright Apache-2.0 conservés dans le fichier. Cette adaptation doit être réévaluée lors d'un changement de SDK.

Les tests exécutent le véritable module source avec des verrous système simulés : le témoin SDK autorise l'entrée ECDSA pendant un transfert SHA actif dans 33 scénarios contrôlés ; le module corrigé l'empêche. Les 8 000 acquisitions concurrentes SHA/AES, ECDSA, DS et ECC terminent sans blocage et sans chevauchement SHA/ECDSA dans le module corrigé. Les variantes autre cible et futur SDK laissent les symboles au SDK. Il s'agit d'une preuve du défaut de synchronisation et de sa correction, pas de la reproduction du reset observé sur la tablette.

## Informations après un reset

Le contexte RTC passe au format 5 : huit emplacements dans 976 octets, dont un emplacement Git distinct du coffre. Les détails sont limités à 80 octets pour les autres activités. Les champs de détail du coffre et de Git sont toujours ignorés ; seuls des noms de phase constants doivent leur être transmis. Aucun PIN, jeton, mot de passe ou URL de dépôt n'y est conservé. Aucun accès SD n'est nécessaire.

Les phases demeurent consultables dans les informations système après les redémarrages compatibles avec la rétention RTC. Elles indiquent le dernier travail observé, jamais à elles seules la cause du reset. Les anciens formats, les données corrompues et les démarrages sans rétention fiable sont rejetés. Les tests couvrent les 976 octets individuellement, les huit activités simultanées, les mises à jour concurrentes et l'exclusion des détails confidentiels.

## Limites de vérification

Aucune tablette physique n'a exécuté ces essais. L'audit du pilote AES du SDK exact montre déjà des copies intermédiaires pour certaines bornes non alignées ; un alignement du tampon de l'application ne suffit donc pas à démontrer la cause des resets. L'absence d'exclusion ECDSA/SHA/AES est un défaut concret corrigé ici, mais elle n'explique pas nécessairement un reset sans opération cryptographique concurrente.
