Tests de transitions du coffre — hôte C++14

Commande : python run.py --cxx chemin/vers/g++

Vérifie : coffre neuf puis fermeture/réouverture, ancien coffre inchangé, coffre
indépendant, fusion des clés, limite de capacité sans écrasement, échec d’écriture
sans perte de session, refus d’un bloc appareil corrompu, mauvais ancien secret.

Les API Arduino, NVS, RTOS et de chiffrement sont simulées pour vérifier les
transitions et la conservation des données. Cela ne valide ni la cryptographie,
ni les performances NVS, ni la concurrence ou l’interface sur un Tab5 réel.
Le script copie à chaque exécution les vraies sources courantes du coffre.
