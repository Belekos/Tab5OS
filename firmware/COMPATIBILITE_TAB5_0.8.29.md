# Compatibilité Tab5 — 0.8.29

## ESP32-C6 et Wi-Fi

Le P4 utilise le pilote Wi-Fi du SDK et le transport ESP-Hosted sur SDIO pour communiquer avec le C6. La 0.8.29 retire le contrôle ajouté par Tab5OS qui refusait une recherche ou une connexion lorsque la majeure C6 différait. Elle retire aussi la requête RPC de version préalable aux opérations Wi-Fi.

| Information C6 | Comportement de Tab5OS 0.8.29 |
| --- | --- |
| Version inconnue ou illisible | Le diagnostic ne bloque pas la recherche ou la connexion |
| Version connue et différente | Aucune interdiction fondée sur cette différence |
| Version concordante | Pas de garantie de bon fonctionnement à elle seule |
| Échec réel du transport ou du pilote | L’opération peut échouer ; le diagnostic de version ne le masque pas |

La 0.8.29 n’ajoute ni traduction entre protocoles ni prise en charge universelle des firmwares C6. ESP-AT, un firmware destiné à un autre bus ou une variante incompatible d’ESP-Hosted ne deviennent pas interchangeables avec le firmware attendu par le SDK. Le C6 n’est pas flashé automatiquement. L’objectif de ce correctif est de laisser fonctionner les firmwares déjà exploitables par le pilote, sans interdiction supplémentaire fondée sur leur numéro.

## Écran et tactile

Le choix du pilote reste confié à M5GFX. Les diagnostics existants des variantes ILI9881C/GT911, ST7121 et ST7123 sont conservés ; une valeur inconnue reste signalée comme telle. La 0.8.29 ne force aucun autre pilote, ne reprogramme pas le tactile et ne modifie pas les broches de reset ni les paramètres DSI.

La présence des pilotes et de l’autodétection ne remplace pas une vérification sur chaque variante matérielle. Les dépendances conservées sont documentées dans [COMPILATION.md](COMPILATION.md).

## Révision ESP32-P4

L’image 0.8.29 a été compilée et vérifiée pour la cible Tab5 `ChipVariant=prev3` du SDK M5Stack 3.3.9, avec les bibliothèques `esp32p4_es-libs`. Son en-tête déclare des bornes de révision silicium 0.0 à 655.35 : ces valeurs larges ne prouvent pas la compatibilité avec toutes les révisions P4. Le champ distinct de révision des blocs eFuse ne doit pas être interprété comme une liste de processeurs pris en charge. Une autre génération matérielle nécessite une cible et des essais adaptés. Les contrôles du chargeur de démarrage ne sont pas contournés ; aucun chargeur de démarrage de remplacement n’est livré.
