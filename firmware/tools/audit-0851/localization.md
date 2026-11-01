# Localisation et ressources — 0.8.51

Le point de départ est l’archive livrée 0.8.50, SHA-256
`ea13e38382767ee6e061d5a099b553f6a42461454b1a5bbd7bc653893a802c3b`.

Les profils, unités, hauteur signée, azimut et intensité de Dessin sont traduits
dans les quatorze packs. « Relief » n’est plus traduit comme une surface
nécessairement en saillie : une hauteur négative produit aussi un creux. La
nouvelle page d’aide « Dessin : sélection et relief » précise que le traitement
simule l’éclairage de pixels existants, sans créer de géométrie 3D. Elle décrit
le repère de l’azimut et la proportion de la distance maximale au contour. Le
profil circulaire est décrit comme arrondi, car sa hauteur reste réglable : il
ne représente pas nécessairement une calotte sphérique exacte.

Les anciennes valeurs, les métadonnées de langues et les configurations de
clavier sont conservées, à deux exceptions explicitement vérifiées : la
traduction du libellé « Relief » et un ancien paragraphe d’aide Explorateur qui
n’est plus référencé. L’aide Explorateur actuelle reste identique. La clé retirée
porte l’empreinte SHA-256
`760d7535f17d17234ba1f3e9190edc127e434ee4503a62112b192c4d3ecb746e`.
Son absence est contrôlée dans les sources C/C++, les scripts Python et les
applications ELF distribuées, hors dictionnaires générés et Factory historique.
Aucun fichier ni pack personnel de l’appareil n’est modifié par ces scripts de
préparation de livraison.

Le pack russe était déjà compact : 523 250 octets sur la limite de 524 288.
Après les ajouts et la suppression de ce paragraphe obsolète, il occupe 519 596
octets, soit une marge de 4 692 octets. La limite du lecteur, son allocation en
PSRAM, le nombre maximal de traductions et son chargement n’ont pas été changés.
Le catalogue contient 3 646 clés de référence. Les vérifications couvrent les
quatorze langues, les formats de paramètres, les sauts de ligne des menus,
les glyphes présents dans les polices distribuées et les claviers disponibles.

Les ressources Factory 0.8.51 comprennent 180 fichiers. Les 40 ressources de
secours intégrées représentent 382 257 octets avant compression, 115 875 après.
La reconstruction conserve les empreintes de la version précédente pour la
procédure existante de mise à jour des ressources officielles.

Les essais de `tools/locale-tests-0851/run.py` compilent la version réelle de
`Tab5Localization.cpp`, son lecteur de langue et les dictionnaires générés,
sous ASan/UBSan. Ils sélectionnent les quatorze packs et vérifient chaque
traduction contre ArduinoJson, les traductions identiques explicites, le repli
sans police, le français personnalisé, les quatre refus d’allocation, une
lecture interrompue, les entrées invalides et les fichiers dépassant la limite.
Les doubles de système de fichiers, préférences et sémaphores sont limités à
cet essai sur ordinateur. Ils ne mesurent pas la latence SD ou le rendu sur Tab5.
Le résultat exécuté et ses empreintes figurent dans `tools/verification/locale-0851.json`.

L’audit différentiel final doit être relancé après gel de toutes les sources
avec `tools/localization/audit_current.py --baseline <archive-0.8.50>` : ses
empreintes incluent les sources UI et moteur qui peuvent encore évoluer en
parallèle. Les preuves historiques ne sont pas renommées en preuves 0.8.51.
