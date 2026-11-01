Tests hôte de l'implémentation réelle Tab5FileOperation.cpp, avec faux RTOS synchrone et appels POSIX instrumentés.
Commande : python run.py --cxx g++
Vérifie : fin prématurée lecture, disque plein, écritures courtes, échec fsync, corruption détectée à la relecture, annulation, source modifiée, collision et dossier imbriqué.
Ces tests ne valident pas les durées, le pilote FAT/USB, les contraintes mémoire ou les tâches concurrentes sur le Tab5.
Le worker ne supprime jamais les sources ; le déplacement inter-volume produit une copie vérifiée, original conservé.
