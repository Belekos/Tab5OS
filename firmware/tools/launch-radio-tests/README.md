# Lancement Python, taille d’éditeur et recherche radio

Exécuter `python -X utf8 tools/launch-radio-tests/run.py` depuis la livraison.
Le banc Windows utilise MSVC 2022 et LVGL 9.6 installés dans les emplacements
de validation du projet (`C:/workspace/tab5-deps-0815/lvgl` et la bibliothèque
LVGL native `C:/workspace/tab5-ui-0819-host/lvgl.lib`).

Le script extrait les méthodes effectivement livrées : exécution de l’éditeur,
reprise de l’OS et de la fenêtre Python, fin de tâche, changement de taille.
Il les exécute avec des frontières VM/fenêtre simulées. La mise en page utilise
les vrais WriterView, WriterRich, WriterDocument et LVGL. Les contrôles vérifient
notamment la prise de premier plan après sauvegarde, le retour au menu précédent,
les échecs de lancement, le maintien de sélection et du défilement, le zoom local
et l’absence de chargement de police quand l’éditeur est caché.

`radio.py` importe le vrai module `tab5catalogs.py` avec les seules frontières
API, fichiers et widgets simulées. Il vérifie les paramètres de recherche,
le choix Tous/MP3/AAC, le rejet des flux incompatibles et HLS, la pagination,
l’import manuel et la suppression du fichier de réponse temporaire.

Le résultat et les empreintes des sources sont dans
`tools/verification/launch-radio-0821.json`. Aucun accès au matériel ou au réseau
réel : ce banc ne prouve ni le rendu physique ni le décodage radio sur la tablette.
