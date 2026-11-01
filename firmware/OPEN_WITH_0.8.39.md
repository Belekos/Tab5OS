# Ouvrir avec — 0.8.39

L’explorateur permet de choisir explicitement une application pour le fichier sélectionné, indépendamment de son extension. Le chemin reste visible pendant le choix.

- **Application native** : Éditeur, Lecteur, Images, Dessin, Audio ou Vidéo. Le module choisi vérifie le contenu et peut refuser un format incompatible.
- **Application ELF** : choisir un fichier `.elf`, vérifier le document prérempli dans le lanceur, puis lancer. L’application reçoit son chemin dans `argv[1]`, suivant l’API Tab5OS existante.
- **Script Python** : choisir un fichier `.py`. Le script reçoit son propre chemin dans `sys.argv[0]` et le chemin du document dans `sys.argv[1]`. Les chemins SD et USB suivent la notation logique Tab5OS (`/Documents/...`, `/USB/...`).

Exemple minimal pour un script qui sait lire des documents texte :

```python
import sys

if len(sys.argv) > 1:
    with open(sys.argv[1], "r") as document:
        print(document.read(4096))
```

Le fichier n’est ni importé comme code ni inséré dans un préambule Python. Le chemin est transmis comme une valeur distincte ; les espaces, accents et apostrophes n’exigent aucun échappement. Une application Python déjà active reste active tant que son remplacement n’a pas été confirmé. Choisir le même script avec un autre document propose aussi ce remplacement.

Les associations ordinaires de l’explorateur ne sont pas modifiées par ce choix. Un script ou un ELF doit lire l’argument pour ouvrir le document ; le lanceur ne transforme pas automatiquement son format.
