# Réveil

Depuis l’accueil, dans les applications intégrées, ouvrir Réveil. Vérifier d’abord la date, l’heure et le fuseau horaire dans Réglages. Activer l’interrupteur, choisir l’heure, Une fois ou Tous les jours, puis Enregistrer. Prochain réveil affiche l’échéance réellement enregistrée. Un changement de champ n’est appliqué qu’après Enregistrer.

Le volume du réveil est séparé du volume habituel. Essayer la sonnerie enregistre les réglages et joue dix secondes ; Arrêter termine immédiatement l’essai. Vérifier le son avec la sortie audio souhaitée : le routage habituel vers le haut-parleur ou le casque est conservé.

À l’heure prévue, l’écran se rallume et la sonnerie joue pendant cinq minutes au maximum. Arrêter ou Échap la termine. Reporter de 5 minutes, ou Espace pendant la sonnerie, programme un nouveau rappel. Une lecture audio peut être suspendue puis reprise par le pilote partagé ; vidéo, application ELF et enregistrement en cours reçoivent une demande d’arrêt pour laisser la sortie sonore disponible. Un échec de son reste visible à l’écran.

Les réglages sont mémorisés en mémoire interne. Une échéance dépassée de moins de dix minutes peut sonner après un redémarrage ; une échéance plus ancienne est signalée comme manquée et n’entraîne pas une sonnerie plusieurs jours après. Une alarme quotidienne est recalculée en heure locale et ne sonne qu’une fois dans l’heure répétée lors du passage à l’heure d’hiver.

## Choisir la veille

La mise en veille habituelle de l’écran conserve les applications en cours. Le réveil y fonctionne également, mais l’OS continue de tourner.

Veille jusqu’au réveil permet une économie plus forte. Enregistrer les documents, terminer les applications/transferts et éjecter proprement tout stockage USB. L’OS refuse de s’arrêter lorsqu’une activité ou une écriture reste en cours. Si le Wi-Fi est actif, le premier appui demande son arrêt ; réappuyer quand il est arrêté. Le réveil doit être enregistré et distant d’au moins trois minutes.

La RTC RX8130CE conserve le compte à rebours et commande le réveil matériel. Le P4 et le C6 ne restent pas actifs pour compter les secondes. Le redémarrage est programmé environ deux minutes avant l’alarme pour absorber l’arrondi matériel et le temps de démarrage ; la sonnerie attend ensuite son heure. La session des applications en RAM ne reprend pas. Un appui sur le bouton permet aussi de rallumer la tablette.

Garder une batterie chargée ou une alimentation branchée. La charge de la batterie est interrompue quand le Tab5 est éteint : pour charger durant la nuit, utiliser la veille de l’écran. Le double appui matériel d’extinction n’est pas équivalent au bouton Veille jusqu’au réveil et ne programme pas ce réveil.

Avant de s’en servir comme réveil quotidien, faire un essai à quelques minutes sur la tablette, puis un essai de veille prolongée sur batterie et/ou sur alimentation. La consommation réelle et le réveil physique doivent encore être mesurés.

Référence matérielle : https://docs.m5stack.com/en/core/Tab5 (RTC RX8130CE, commande de réveil et charge appareil allumé).
