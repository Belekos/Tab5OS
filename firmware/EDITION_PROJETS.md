# Édition, code et projets — v0.8.19

L’application Écriture est aussi l’éditeur de code. Les fichiers Python, C/C++, Arduino, JavaScript/TypeScript, JSON, HTML/CSS, XML, YAML, TOML, scripts et configurations usuelles s’ouvrent dans le même éditeur. Leur source reste intégralement visible : `#`, `*`, liens et tableaux ne sont jamais interprétés comme du Markdown. La coloration est seulement visuelle ; elle n’ajoute aucune balise au fichier. Les documents Markdown conservent leur édition mise en forme, avec ajout du souligné et du barré.

Le menu **Code** rassemble Nouveau script Python, Exécuter, Arrêter, Reprendre, Bibliothèques et Console. Exécuter enregistre d’abord la version courante, vérifie la sauvegarde, puis lance le fichier `.py` enregistré. Un échec de sauvegarde annule l’exécution. La limite d’exécution est de 64 Kio ; l’éditeur accepte les fichiers UTF-8 jusqu’à 2 Mio. La console se place à droite, conserve au plus 8 Kio et n’est actualisée que lorsqu’elle est visible, au maximum quatre fois par seconde.

**Documents** affiche les fichiers inscrits au groupe choisi, même lorsqu’ils ne sont pas chargés. Le groupe « Hors projet » reste disponible. La croix retire uniquement l’inscription ; elle ne supprime jamais le fichier SD. Changer de document enregistre d’abord les modifications et la position. Une sauvegarde refusée laisse le document courant ouvert.

Pour créer un projet, ouvrir le document puis choisir **Documents → Créer un projet avec ce document**. Son inscription initiale hors projet est conservée. Le choix du groupe permet ensuite **Ajouter le document au projet**. **Importer un dossier** associe le dossier au projet sélectionné, ou crée un projet lorsqu’aucun n’est sélectionné. Les sous-dossiers sont parcourus progressivement ; une nouvelle analyse toutes les 30 secondes, pendant que l’éditeur est visible, détecte les nouveaux fichiers. Une inscription retirée par la croix n’est pas réintroduite par ces analyses ; une ouverture explicite peut la réinscrire.

L’ensemble est limité à 8 projets, 256 inscriptions de documents et 16 lignes de liste par page. Les chemins acceptent 299 octets UTF-8. Une analyse visite au plus 4 entrées par passage, 20 000 entrées par analyse et 8 niveaux de sous-dossiers. Les dossiers cachés, `node_modules` et `__pycache__` sont ignorés. Les fichiers de configuration utiles comme `.env` et `.gitignore` restent admis.

| Raccourci | Action |
|---|---|
| Ctrl+C / X / V / A | Copier / couper / coller / tout sélectionner, dans le document entier |
| Ctrl+Inser / Maj+Inser / Maj+Suppr | Copier / coller / couper |
| Inser | Basculer insertion / remplacement |
| Maj + déplacement | Étendre la sélection |
| Ctrl+Début / Fin, avec Maj pour sélectionner | Début / fin du document |
| Ctrl+Maj+N | Numéros de lignes, pour tous les types de fichier |
| Ctrl+U / Ctrl+Maj+X | Souligné / barré dans les documents mis en forme |
| Ctrl+Entrée | Enregistrer puis exécuter le fichier Python |
| Ctrl+. | Arrêter l’exécution |
| Ctrl+Maj+R | Reprendre |
| Ctrl+Maj+L ou Ctrl+` | Afficher / masquer la console |
| Ctrl+Maj+O | Ouvrir le menu Documents |
| Ctrl+Tab / Ctrl+Maj+Tab | Document suivant / précédent du groupe sélectionné |
| Ctrl+S / Ctrl+Maj+S | Enregistrer / enregistrer sous |
| Ctrl+O / Ctrl+N | Ouvrir / nouveau document |
| Échap | Revenir aux commandes / fermer le menu |

Les numéros de lignes occupent une marge étroite au fond distinct. Les menus adaptent leur largeur aux traductions et réservent une place séparée à leur flèche. Un contour indique la zone en cours de saisie. Les sauvegardes de secours des documents se trouvent dans `/.tab5-backups`, selon l’arborescence du fichier original ; les anciennes sauvegardes voisines restent récupérables. Le Markdown conserve le souligné avec `<u>…</u>` et le barré avec `~~…~~`. Les fichiers de code restent bruts.

La liste, les groupes et les positions sont sauvegardés dans `/System/editor_workspace.dat` par une transaction vérifiée, avec une version précédente. La reprise du document porte son identifiant de projet explicite : après une coupure entre deux sauvegardes, un ancien document n’est jamais rattaché au projet actuellement sélectionné. Une reprise n’est acceptée que si l’inscription exacte existe encore. Le dernier document retiré ne réapparaît donc pas à cause d’une ancienne session. Aucune liste personnelle n’est fournie dans le paquet initial.

# Editor, source files and projects — v0.8.19

Writing is also the code editor. Python and common source/configuration formats use the same editing surface, displaying every source character without Markdown interpretation. Syntax colours are visual only and never add markup to saved files. Markdown documents retain rich editing.

Use **Code** for New Python script, Run, Stop, Resume, Libraries and Console. Run first completes a verified save and then executes the saved `.py` file. Failed saves cancel execution. Editing is limited to 2 MiB of UTF-8; Python execution is limited to 64 KiB. The right console holds at most 8 KiB and only refreshes while visible, at most four times per second.

**Documents** keeps unloaded documents in a persistent list. Select a project or the standalone group. The X removes membership only, leaving the SD file intact. Switching saves edits and the current position first; a failed save keeps the active document open. Creating a project from a document preserves its standalone membership. Add the current document to the selected project or import a folder; visible-editor rescans discover new files every 30 seconds. Removed memberships are excluded from automatic rescans.

Limits: 8 projects, 256 document memberships, 16 visible document rows per page, 299-byte paths, 4 scanned entries per service pass, 20,000 entries per scan and 8 subfolder levels. Hidden directories, `node_modules` and `__pycache__` are skipped; useful dotfiles such as `.env` and `.gitignore` are accepted. Cursor, selection and scroll positions are restored per membership. Session recovery validates an explicit project ID and existing membership, preventing cross-project reassignment or resurrection after removing the last document.

Shortcuts: Ctrl+Enter saves and runs Python; Ctrl+. stops; Ctrl+Shift+R resumes; Ctrl+Shift+L or Ctrl+` toggles the console; Ctrl+Shift+O opens Documents; Ctrl+Tab and Ctrl+Shift+Tab cycle through the selected group. Standard save, search, undo and editing shortcuts remain available.

Shared clipboard: Ctrl+C/X/V/A, Ctrl+Insert, Shift+Insert and Shift+Delete. Shift with navigation extends selection; Ctrl+Home/End targets the whole document. Insert toggles overwrite. Ctrl+Shift+N shows the narrow line-number gutter. Ctrl+U and Ctrl+Shift+X add underline/strikethrough to formatted documents. New document backups use `/.tab5-backups`.
