# Dessin — nouveautés 0.8.34

Cette version part de la 0.8.33, pour laquelle l’utilisateur a confirmé le fonctionnement de Git et l’absence de nouveaux flashs dans son usage. La politique mémoire, Git et le pilote d’affichage sont préservés ; la 0.8.34 nécessite ses propres essais sur tablette.

L’application s’appelle **Dessin**, avec traduction dans les 14 langues de l’OS. Le rail d’icônes indique l’outil actif et le panneau droit présente ses réglages. Fichier, Édition, Affichage, Image et Calques regroupent les actions générales. Les couleurs restent visibles.

L’opacité ne s’accumule plus à chaque empreinte d’un même trait. Dureté du bord et diffusion des points ont des effets distincts. Six formes sont proposées : rond, carré, barres horizontale et verticale, deux diagonales. La déformation locale ajoute pousser, gonfler et pincer avec historique et annulation.

Le zoom est ancré au pointeur. Main ou bouton central déplace la vue sans éditer les pixels. Le clic droit donne accès aux commandes contextuelles. Les aperçus correspondent à la ligne, au rectangle ou à l’ellipse ; les options d’ellipse proposent cercle et tracé depuis le centre. La barre inférieure affiche les coordonnées et mesures pendant le geste.

Le tampon de vue reste de taille bornée, indépendante du zoom. Les déplacements immobiles ne recomposent pas l’image. Les changements de paramètres ou de calques interrompent proprement un geste en cours. L’interruption par un dialogue ne doit pas laisser un déplacement capturé.

Les imports JPEG standard et gris et l’export JPEG complètent BMP et T5I. Un import invalide ou refusé par l’allocateur conserve l’ancien document. Le décodage utilise une réduction native pour les grandes images. JPEG progressif et orientation EXIF automatique ne sont pas pris en charge.

Deux défauts du décodeur JPEG amont ont été reproduits et corrigés : décalages de nombres négatifs et copie de mémoire avec chevauchement au bord de certaines images. Les correctifs sont appliqués à une copie isolée de M5GFX lors de la compilation.

Mettre à jour firmware et ressources SD ensemble selon [LISEZ_MOI.md](LISEZ_MOI.md). Voir [DESSIN.md](DESSIN.md) pour l’usage et [VERIFICATION.md](VERIFICATION.md) pour les tests effectivement exécutés sur les sources reconstruites.
