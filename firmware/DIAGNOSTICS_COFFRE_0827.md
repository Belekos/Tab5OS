# Contexte du coffre après un redémarrage

La version 0.8.27 réserve au coffre un marqueur de phase dans la mémoire RTC. Après un redémarrage compatible avec sa conservation, le contexte précédent apparaît dans les informations système existantes et dans la sortie de diagnostic au démarrage. Il indique la dernière phase observée, jamais une cause certaine du redémarrage.

Les appels du coffre utilisent uniquement des noms de phase constants. Le champ de détail de son emplacement est systématiquement ignoré : aucun PIN, identifiant, mot de passe, nom de fichier ou adresse n'y est enregistré. Ces marqueurs n'écrivent rien sur la carte SD.

Le format RTC passe de 3 à 4 et contient sept emplacements, pour 968 octets au total. Un ancien format, un enregistrement incomplet ou corrompu et les démarrages dont la conservation RTC n'est pas fiable sont rejetés. Une coupure d'alimentation peut donc ne laisser aucun contexte exploitable.

Le parcours interactif doit conserver son marqueur jusqu'à la fin de la reconstruction de l'écran du coffre. La fin du worker seule ne signifie pas que ce passage d'écran est terminé. Les appels sans interface terminent leur marqueur quand leur travail est achevé. Les marqueurs identiques ne réécrivent pas le bloc RTC.
