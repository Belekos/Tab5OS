# Vérification USB HID et XInput — 0.8.23

Depuis une invite Windows disposant de Python et MSVC 2022 :

```text
python tools/usb-gamepad-tests/run_host.py Tab5OS_Arduino_v0.8.23_Ecriture --baseline ../Tab5OS_Ecriture_v0.8.22/Tab5OS_Arduino_v0.8.22_Ecriture
```

`--baseline` est facultatif. Le lanceur ouvre lui-même l'environnement MSVC. Le test compile les vraies fonctions du pilote (`hid_host.c`) extraites sans changement de corps, avec doubles USB/RTOS et injection d'échecs d'allocation. Le parseur de configuration et les décodeurs HID/XInput sont compilés directement. Le rapport conserve les empreintes de tous les fichiers de production concernés. Il n'est pas une validation matérielle ni une mesure SRAM du Tab5.

Les fixtures représentent un périphérique composite clavier/souris/manette/disque et des rapports HID/XInput normalisés. Elles ne prétendent pas être une capture SN30 Pro. Le test échoue si une allocation reste retenue, si une interface reste revendiquée, si le parent est libéré trop tôt, ou si le parseur traverse les limites d'une interface.

La 0.8.23 ajoute deux bancs :

```text
python tools/usb-gamepad-tests/run_pipeline.py Tab5OS_Arduino_v0.8.23_Ecriture
python tools/usb-gamepad-tests/run_settings.py Tab5OS_Arduino_v0.8.23_Ecriture
```

`run_pipeline` extrait sans changer leur corps le callback de réception, la remise à zéro et les API de lecture de `WriterUsb.cpp`. Les décodeurs HID/XInput sont les unités de production. L’injection des octets USB et les files RTOS sont des frontières simulées ; il n’y a aucun appel à F1. Le format de neuf octets est basé sur `HIDAPI_Driver8BitDo_HandleOldStatePacket` du projet SDL et ne s’applique qu’aux VID/PID USB SN30/SF30 Pro connus.

`run_settings` compile l’inclusion d’interface de production avec LVGL réel ; les aides de création des contrôles et la persistance USB sont simulées. Il utilise la bibliothèque LVGL native du banc UI existant, dont l’empreinte est conservée. Ce test ne remplace ni la compilation du firmware ni un essai matériel.
