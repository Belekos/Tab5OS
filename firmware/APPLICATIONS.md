# Applications et accueil — 0.8.53

L’accueil présente les applications intégrées ainsi que les icônes **Apps ELF** et **Apps Python**. Ces deux icônes ouvrent des menus séparés ; Retour retrouve l’accueil et sa position de défilement. Les applications sont classées par nom dans leur menu. Les outils intégrés restent disponibles sans carte SD. F5 relit le catalogue local après ajout ou retrait de fichiers ; il ne s’agit pas d’un magasin en ligne.

**Réglages → Stockage** permet de reconnecter ou éjecter la microSD, de gérer le disque USB détecté et de recréer les ressources système manquantes. Cet onglet reste disponible sans les modules Python de la carte ; voir REPARATION_SD.md, STOCKAGE_SD.md et STOCKAGE_USB.md.

## Personnaliser l'accueil

1. Ouvrir **Réglages → Interface → Affichage → Accueil et bandeau**.
2. Choisir **Personnaliser l’accueil**.
3. Ajouter ou retirer une application de l’accueil, puis utiliser **Monter** et **Descendre** pour son ordre dans sa section.
4. Choisir l’image automatique ou l’un des 24 symboles proposés.
5. Régler la présentation en grille ou liste, la taille compacte, normale ou grande et l’affichage des libellés.
6. Utiliser **Enregistrer** pour conserver les choix. **Retour** abandonne les changements non enregistrés.

Les choix sont enregistrés dans la mémoire interne. **Alt+7** ouvre Réglages même si son raccourci est masqué. **Réinitialiser**, puis **Enregistrer**, restaure la disposition par défaut sans supprimer aucun fichier ni aucune application ; **Retour** abandonne les changements non enregistrés.

Dans **Accueil et bandeau**, **Ajouter une application** propose les applications natives, ELF et Python du catalogue. Le bandeau accueille jusqu’à **10 raccourcis**, sans défilement. Les flèches changent leur ordre et la corbeille retire un raccourci sans supprimer l’application. Ces changements sont enregistrés immédiatement. L’interrupteur **Afficher le bandeau latéral** masque ou réaffiche l’ensemble en conservant ses choix. Si le bandeau est absent, Accueil reste dans la barre supérieure en dehors du menu principal et les pages utilisent la largeur libérée.

Le raccourci d’une application sur un support absent reste mémorisé et devient indisponible. Il redevient utilisable après reconnexion et découverte du même chemin. Ouvrir le sélecteur après l’ajout de fichiers relance la découverte ; aucun scan périodique n’est ajouté à l’accueil natif au repos pour ses raccourcis externes.

## Applications détectées

Les emplacements suivants sont recherchés sur la microSD et sur un disque USB monté (`/USB/…`).

| Emplacement | Contenu reconnu |
|---|---|
| `/MicroPython` | Applications déclarées et scripts immédiats `.py` |
| `/Applications` et `/Apps` | Fichiers immédiats `.py`, `.elf` et `.app.elf` |
| Un sous-dossier de `/Applications`, `/Apps` ou `/MicroPython` | Point d’entrée `main.py`, `main.app.elf` ou `main.elf` |

La recherche est progressive et n’exécute aucun programme. Le catalogue contient au plus **64 entrées**, outils intégrés compris, et parcourt au plus **384 entrées de dossier**. Une limite atteinte est signalée ; les autres fichiers restent accessibles depuis l’Explorateur. F5 relit les emplacements après ajout ou retrait de fichiers.

Un fichier reconnu comme application n'est pas automatiquement compatible : un ELF doit viser l'ABI Tab5OS et un script peut dépendre de modules absents. L'ouverture laisse les contrôles habituels du chargeur ou de l'interpréteur produire une erreur explicite. Une carte SD absente conserve l'accès aux outils intégrés.

Les fichiers de Quake et Duke Nukem 3D ne sont pas fournis dans cette distribution. Leurs ELF suivent les règles ordinaires de découverte du catalogue, sans exclusion spécifique à ces noms ; leur compatibilité est vérifiée au lancement. Les données personnelles et sauvegardes déjà présentes ne sont pas effacées.

## Lancement et retour

Quitter une application Python ferme son écran et revient à l’écran précédent de l’OS, y compris après un lancement depuis l’éditeur ou l’Explorateur. L’écran intermédiaire affichant seulement le chemin du script et Arrêter n’est plus le point de retour normal. **Masquer**, lorsqu’il est proposé, conserve l’exécution ; **Ctrl+Alt+P** permet de reprendre l’application encore active. **Ctrl+.** demande son arrêt.

Les applications ELF disposent de réglages mémorisés par application : demander un fichier au lancement, lancer sans fichier ou toujours utiliser le même fichier. Les chemins publics sont `sd:/Dossier/fichier.ext` et `usb:/Dossier/fichier.ext`. Voir BINAIRES.md.

## Icônes

Le choix **Image ou symbole automatique** cherche dans cet ordre :

1. Le fichier voisin portant le même nom que l'application, sans son extension : par exemple `/Apps/minuteur.py` utilise `/Apps/minuteur.ico`.
2. `icon.ico` dans le dossier propre à une application : par exemple `/Apps/minuteur/main.py` peut utiliser `/Apps/minuteur/icon.ico`.
3. Le nom de l'application dans `/Icons` : `/Icons/minuteur.ico` pour `minuteur.py`, ou pour `minuteur/main.py` et `minuteur/main.app.elf`.

Les outils intégrés utilisent leur identifiant, par exemple `/Icons/files.ico` pour Fichiers. Les icônes non trouvées ou refusées laissent le symbole intégré visible. Choisir un symbole dans la liste prend priorité sur l'image automatique.

Pour limiter mémoire et temps de chargement : fichier ICO de **32 Kio maximum**, jusqu'à **16 images**, image BMP interne de **24 ou 32 bits**, dimensions au plus **64 × 64**. Un ICO contenant seulement une image PNG n'est pas décodé. Les images s'affichent progressivement, une à la fois, avec un budget total de pixels de **192 Kio** ; lorsque ce budget est atteint, les symboles restent affichés.

## Limites

L'accueil configurable ne transforme pas toutes les fonctions du firmware en exécutables externes. Les fonctions natives indispensables demeurent intégrées. Les ELF ne sont pas des processus isolés et peuvent encore affecter le système en cas d'erreur native ; voir BINAIRES.md.

Le catalogue d'applications est distinct du catalogue **Modules et projets**, destiné aux bibliothèques MicroPython et aux projets pour d'autres cartes. Voir MICROPYTHON_BIBLIOTHEQUES.md.

## Dessin (0.8.35)

Dessin est intégré au firmware, avec la police, les contrôles et la palette de l’OS. Il se lance depuis l’accueil ou avec **Ouvrir avec… → Dessin** sur une image dans l’explorateur. Un projet T5I ouvre directement Dessin. L’ancienne application ELF n’est plus nécessaire.

Les outils sont représentés par des icônes. Le volet de réglages est fermé au départ et se superpose au dessin sans modifier son cadrage. Pinceaux, gomme, calques, historique, sélection, formes, texte sur l’image, réglages couleur, déformation locale et transformations globales sont disponibles. La molette zoome ; Main ou bouton central déplace la vue sans peindre. Le bouton gauche utilise la première couleur et le bouton droit la seconde.

Les imports conservent les dimensions originales ou sont refusés explicitement si la mémoire manque. Changer d’application conserve le document ; fermer demande confirmation si l’image est modifiée. Enregistrement explicite en T5I, BMP ou JPEG. T5I conserve les calques ; BMP/JPEG aplatissent le composite sur blanc. Voir [DESSIN.md](DESSIN.md) pour les commandes, les formats et les limites.
