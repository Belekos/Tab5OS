# Régression Wi-Fi 0.8.20

Ces tests compilent le véritable verrou réseau, les corps des méthodes Wi-Fi de
`Tab5Audio.cpp`, le rafraîchissement de la liste Réglages et le contrôle/arrêt du
partage direct. Seules les frontières pilote Wi-Fi, ordonnanceur FreeRTOS et LVGL
sont simulées. Ils ne prouvent pas la réception radio sur la tablette.

Depuis la racine de l'archive 0.8.20, avec Python 3 et Visual Studio 2022 Community
avec les outils C++ installés :

```powershell
& tools\wifi-transition-tests\run_msvc.cmd --report tools\verification\wifi-transitions-0820.json
```

Dans un terminal développeur disposant déjà de `cl`, on peut aussi exécuter
`python -X utf8 tools/wifi-transition-tests/run.py` directement. Le test courant
est autonome et n'utilise aucun chemin vers une version précédente.

La comparaison facultative nécessite de décompresser séparément l'archive
**0.8.19 livrée**, puis d'indiquer son dossier de sketch :

```powershell
& tools\wifi-transition-tests\run_msvc.cmd --baseline ..\Tab5OS_Ecriture_v0.8.19\Tab5OS_Arduino_v0.8.19_Ecriture --report tools\verification\wifi-transitions-0820.json
```

Le test attend exactement trois échecs 0.8.19 : connexion refusée avant
`WiFi.begin`, arrêt refusé avant la fermeture du pilote, partage direct refusé
alors que le Wi-Fi est désactivé. Le rapport conserve ces échecs attendus, le
résultat courant et les SHA-256 des fichiers de production effectivement testés.

Les scénarios courants couvrent connexion, reconnexion mémorisée, absence de
SSID, mauvais mot de passe, arrêt, recherche après arrêt/échec, résultats vides
distincts d'une recherche échouée, publication à l'interface, files de commandes
saturées, contention, délais, annulation et interdiction des clients pendant une
transition. Le partage direct est testé avec arrêt normal et premier refus du
pilote suivi d'une récupération, en exécutant le véritable nettoyage du worker.
Un pilote qui refuserait indéfiniment de s'arrêter conserve un arrêt en attente :
le système ne prétend pas que le réseau est arrêté et n'autorise pas une
transition concurrente.

Le problème introduit en 0.8.19 venait du refus des clients lorsque le réseau
est désactivé : les transitions internes désactivaient justement cette admission
avant d'emprunter le même verrou client. La 0.8.20 sépare leur admission interne,
tout en conservant l'exclusion mutuelle et le drainage des clients existants.
Ce contrôle n'ajoute ni VPN, ni routage, ni isolation réseau des applications.
