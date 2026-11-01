# Clé USB et lecteur de carte sur le Tab5

Brancher une clé USB ou un lecteur de carte USB à l’USB-A, directement ou via un hub compatible. Le montage automatique est activé au démarrage. Ouvrir **Réglages → Stockage**. Le panneau du disque USB et ses actions apparaissent lorsqu’un support est détecté ; les commandes de montage, d’ouverture et d’éjection suivent son état. Le montage automatique reste réglable. Ce choix automatique est conservé pendant la session. Les commandes microSD et de récupération des ressources sont sur la même page.

Un volume monté apparaît sous `/USB`. Dans l’Explorateur, l’icône de stockage avec sa flèche ouvre un menu **Carte SD / USB** : choisir le support pour rejoindre sa racine. La commande permet de revenir à la SD depuis l’USB ; elle disparaît quand seule la SD est pertinente. L’explorateur et l’éditeur permettent lecture, écriture et copie entre ce support et la microSD. Les sélecteurs de fichiers donnent également accès au support. Les services internes de l’OS et leurs réglages restent sur la microSD. Un dossier personnel déjà nommé `/USB` sur la microSD bloque le montage USB ; il n’est pas masqué ni effacé. Le renommer soi-même si nécessaire.

Choisir **Éjecter** avant de retirer le support. L’OS ferme l’aperçu USB, la recherche et le sélecteur de l’explorateur, puis revient hors du volume. Il refuse les nouvelles ouvertures, attend les fichiers USB réellement ouverts, synchronise le disque et indique que le retrait est possible. Les tâches auditées qui utilisent uniquement la microSD ne bloquent plus l’éjection. Un programme ayant un accès brut non suivi conserve un verrou prudent : il doit le libérer avant le démontage. Les documents modifiés ne sont pas effacés ou fermés de force. Un échec de synchronisation ou de fermeture du volume conserve le support monté et affiche une erreur.

Un retrait inattendu invalide les handles de fichiers de l’explorateur/éditeur. Le volume n’est pas réutilisé par un nouveau périphérique tant que les anciennes tâches et fichiers ne sont pas fermés. Si le support est rebranché trop tôt, arrêter la tâche concernée puis débrancher/rebrancher le support.

Limites de cette livraison :

- Une microSD et **un stockage USB** simultanément, FAT16 ou FAT32. Ni exFAT ni NTFS ; 4 Gio moins un octet par fichier FAT32.
- Lecteurs USB de stockage de masse BOT/SCSI, volume FAT ordinaire. Pas de prise en charge UASP ni de sélection de plusieurs LUN/partitions.
- Aucun formatage automatique, même si le montage échoue.
- Hôte USB commun au clavier, à la souris et au stockage. Le mode de compatibilité hub à pleine vitesse (12 Mbit/s) reste le défaut ; le débit utile est inférieur. Le réglage global USB rapide reste disponible sans disque et prend effet au prochain redémarrage, notamment pour le RTL-SDR. Voir aussi [Manettes USB](MANETTES_USB.md). Les claviers/souris FS/LS derrière un hub HS ne sont pas pris en charge par la pile IDF 5.5.4 dans ce mode.
- Pas de conversion du Tab5 en disque USB exposé à un PC : ce document concerne un support branché **au Tab5**. Le partage Wi-Fi existant reste disponible séparément.

Les anciens contrôles sur ordinateur portent sur les commandes, les protections de montage/retrait et le routage des fichiers. Ils ne certifient pas la compilation ni les nouvelles modifications de la 0.8.19. La compatibilité des hubs, claviers, souris et lecteurs utilisés ensemble reste à vérifier sur le matériel de l’utilisateur. Le pilote ne permet pas de promettre qu’un hub précédemment défaillant fonctionne maintenant.

Sources du pilote : [usb_host_msc 1.1.4](https://components.espressif.com/components/espressif/usb_host_msc/versions/1.1.4/readme), [hôte USB ESP-IDF 5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/peripherals/usb_host.html).

## USB-C et Ethernet

Le P4 comporte deux contrôleurs OTG, mais la pile ESP-IDF 5.5.4 utilisée n’autorise qu’un hôte à la fois. Sur le schéma de la Tab5, l’USB-A est relié au contrôleur haute vitesse avec alimentation 5 V de sortie. L’USB-C est relié au contrôleur pleine vitesse, avec résistances CC de périphérique et VBUS d’entrée de charge ; ce n’est pas le même circuit d’alimentation. L’USB-C sert à la charge, à la programmation et au périphérique série USB Serial/JTAG décrit dans [SERIAL_0826.md](SERIAL_0826.md). Ce service série ne crée pas un second hôte USB. Un simple adaptateur ne constitue pas une prise en charge sûre et équivalente de deux ports hôtes.

Une interface Ethernet USB est envisageable avec son pilote réseau et un chipset pris en charge. Espressif fournit un composant USB ECM pour P4 ; cela ne couvre pas automatiquement les adaptateurs Realtek/ASIX ou tous les ports Ethernet de hubs. Aucun pilote Ethernet n’est intégré dans cette livraison. Pour choisir le bon pilote, il faudra le modèle ou l’identifiant USB du hub/adaptateur.

Références : [schéma officiel Tab5](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1132/Tab5_Schematics_PDF.pdf), [USB ECM 0.4.0](https://components.espressif.com/components/espressif/iot_usbh_ecm/versions/0.4.0/readme).
