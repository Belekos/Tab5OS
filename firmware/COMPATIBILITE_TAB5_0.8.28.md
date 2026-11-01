# Compatibilité Tab5 : écran, ESP32-P4 et ESP32-C6

L'écran et le module radio suivent deux chemins distincts. M5GFX sélectionne le pilote écran au démarrage ; le Wi-Fi communique avec le firmware du C6 par ESP-Hosted sur SDIO. Cette version expose les informations disponibles dans les informations système, sans flasher le C6 automatiquement.

## Écrans

M5Stack date le passage au ST7123 du 14 octobre 2025, puis au ST7121 du 28 avril 2026.

| Variante Tab5 | Identification utilisée | Prise en charge logicielle |
| --- | --- | --- |
| ILI9881C et GT911 | Tactile actif à l'adresse 0x14 | Pilotes M5GFX existants |
| ST7121 | Tactile actif à 0x55, version 1 | Pilotes M5GFX existants |
| ST7123 | Tactile actif à 0x55, version 3 | Pilotes M5GFX existants |
| Autre valeur ou lecture échouée | « Non identifié » | Aucun pilote forcé |

Le projet utilise M5Unified 0.2.23 et M5GFX 0.2.30. Ces versions contiennent le traitement du reset écran recommandé par M5Stack et l'autodétection des trois variantes ci-dessus. Le diagnostic ajouté lit le périphérique tactile déjà choisi par M5GFX ; il ne reconfigure ni la liaison DSI, ni les broches de reset, ni le firmware du tactile. Le couple ILI9881C/GT911 est identifié d'après l'association documentée pour la Tab5, sans nouvelle lecture DSI.

L'écran ST7121 n'utilise pas la même vitesse de liaison que le ST7123. Forcer un pilote au seul motif que le tactile répond à 0x55 serait incorrect ; une version inconnue reste donc signalée comme inconnue. Cette version ne remplace pas une validation physique de chaque lot de fabrication.

## ESP32-C6 et ESP-Hosted

Le composant hôte fourni par le SDK de cette compilation est ESP-Hosted 2.12.8. La version du C6 est demandée après activation du Wi-Fi, depuis son worker, puis conservée en mémoire. Ouvrir les informations système ne déclenche aucun appel réseau. Une nouvelle initialisation du C6 ou une interruption du transport invalide le résultat précédent ; une réponse arrivée après cette invalidation est ignorée.

| Résultat de lecture | Comportement |
| --- | --- |
| Même majeure, mineure et correctif | Versions concordantes ; Wi-Fi autorisé |
| Même majeure, mineure ou correctif différent | Compatibilité à vérifier ; Wi-Fi autorisé |
| Majeure différente connue | Nouvelle recherche/connexion refusée avec un message explicite |
| Version indisponible ou invalide | Diagnostic inconnu ; pas de blocage spéculatif d'un ancien firmware d'usine |

Des versions concordantes ne garantissent pas à elles seules la stabilité radio : le transport SDIO, sa configuration, les fonctions compilées et le matériel comptent aussi. Le diagnostic indique l'état du transport, les échecs observés et, lorsqu'il est reçu, le motif de redémarrage du C6. L'activation initiale reste exécutée par le SDK ; une panne interne au pilote avant la réponse du C6 ne peut pas être empêchée par une comparaison de versions qui n'a pas encore eu lieu.

Pour une mise à jour manuelle, employer une image C6 prévue pour la Tab5, le transport SDIO et la version hôte correspondante. Une image ESP-AT, une image pour un autre bus ou un firmware ESP-Hosted 3.x ne doit pas être considéré comme interchangeable avec le firmware attendu par ce SDK. Aucun binaire C6 non validé n'est ajouté à cette distribution.

## Révision ESP32-P4

La compilation fournie utilise le SDK `esp32p4_es-libs` M5Stack 3.3.9. Sa plage de révisions déclarée est **0.1 à 1.99** (`ESP_REV_MIN_FULL=1`, `ESP_REV_MAX_FULL=199`, représentation majeure × 100 + mineure). Les informations système montrent la révision réelle et les bornes compilées. Une variante de silicium hors de cette plage nécessite une compilation et une validation adaptées ; changer seulement de pilote écran ne suffit pas. Les contrôles de révision du chargeur de démarrage ne sont pas contournés.

## Vérification

Les tests exécutent le module de production et sa politique avec les seules frontières matérielles simulées : lecture I2C, API d'information du silicium, RPC/événements ESP-Hosted et section critique. Ils couvrent les variantes d'écran, les lectures inconnues, les anciennes versions C6, une majeure différente, les réponses invalidées par un reset, les notifications concurrentes et l'absence d'accès matériel lors des lectures de diagnostic. Le module et les hooks du worker audio sont compilés séparément avec les véritables en-têtes du SDK cible. Aucun de ces tests ne constitue une mesure sur tablette.

Sources techniques : [documentation officielle Tab5](https://docs.m5stack.com/en/core/Tab5), [dépannage officiel ESP-Hosted](https://github.com/espressif/esp-hosted-mcu/blob/main/docs/troubleshooting.md), API et configuration du SDK M5Stack 3.3.9 installé, sources M5GFX 0.2.30 utilisées par la compilation.
