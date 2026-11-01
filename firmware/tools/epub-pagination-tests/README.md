# Pagination EPUB 0.8.21

`python -X utf8 tools/epub-pagination-tests/run_layout.py` lance les contrôles
natifs Windows/MSVC. Les chemins de LVGL et du répertoire de compilation sont
en tête du script. Le premier lancement compile LVGL 9.6 avec les options texte
du `lv_conf.h` livré : retours à la ligne, BIDI et façonnage arabe/persan.
Les fontes sont celles des ressources SD (principale et replis japonais/coréen).
Le cache de compilation hôte doit être supprimé ou renommé si ces options changent.

Le test extrait littéralement les fonctions de pagination et de rendu du sketch,
ainsi que les structures du parseur, puis exécute les vrais widgets de spans LVGL.
Il vérifie les limites réelles des objets rendus, le texte de chaque span après
façonnage, les frontières UTF-8, l'absence de perte ou de duplication entre pages,
les compteurs du livre et les positions sauvegardées. Il couvre citations,
retraits, paragraphes, retours explicites, tailles 20/24/28/32, styles mixtes,
longs mots, CJK, arabe/persan, et une marge finale dépassant l'espace restant.
Une panne d'allocation de la mesure est également injectée avec les vraies
fonctions de cache et de navigation : elle ne publie aucune frontière inventée,
ne change ni la page visible ni le chapitre, et ne déplace pas le marque-page.

Sur un long paragraphe, le nombre de mesures et d'objets temporaires par page est
contrôlé ; les tailles de texte présentées au moteur restent bornées à quelques
pages, sans remesure intégrale du paragraphe. Les allocations auxiliaires sont
libérées lors de la fermeture. Les temps imprimés sont ceux du PC hôte.

Si la livraison précédente existe à côté sous `Tab5OS_Ecriture_v0.8.20`, le même
test reproduit ses débordements et inscrit cet échec attendu dans le rapport.
Cette comparaison est facultative ; le test courant reste autonome.

Rapport : `tools/verification/reader-layout-0821.json`. Il contient les empreintes
des fichiers réellement exécutés. Il ne constitue pas un essai sur tablette.

Le schéma de cache de pagination passe à `0x08210001` : les anciens totaux sont
recalculés. Les positions de lecture chapitre/bloc/octet restent compatibles.
