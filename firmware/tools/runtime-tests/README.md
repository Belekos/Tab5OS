# Contrôles hôtes du chargeur et des chemins

`runtime_qa.cpp` se compile avec `Tab5ElfValidate.cpp` et `Tab5ResourceManager.cpp`, en ajoutant le dossier du sketch aux includes. Passer les douze fichiers `sdcard/Applications/*.elf` comme arguments du programme. Les ELF sont analysés, jamais exécutés. Les réservations mémoire utilisent un échantillonneur simulé.

`import_paths.cpp` est un instantané de la fonction `script_import_directory` extraite de `src/micropython/tab5_module.c` pour cette vérification. Il se compile seul ; il ne valide pas la VM complète.

Les logs fournis proviennent de MSVC 2022, avant le renommage final du dossier du sketch ; leurs chemins historiques ne sont pas des commandes à recopier. Aucun test ne certifie la fluidité ou les périphériques physiques.
