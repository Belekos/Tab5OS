# Batterie — Tab5OS 0.8.28

La protection ajoutée surveille la tension du pack et le courant réellement mesuré. Elle avertit avant la décharge profonde puis demande l'arrêt matériel. Elle est automatique sur le Tab5 d'origine, même si l'option d'affichage « Batterie installée » n'a pas été cochée. Elle ne modifie ni le courant de charge, ni la tension de charge, ni les protections du pack.

## Comportement

| Observation continue | Action |
| --- | --- |
| Pack à 7,00 V ou moins pendant 15 s, courant de décharge confirmé | Avertissement invitant à brancher et enregistrer |
| Pack à 6,80 V ou moins pendant 10 s | Décompte de 30 s avant demande d'arrêt |
| Pack à 6,40 V ou moins pendant 3 s | Décompte réduit à 5 s |
| Tension revenue à 6,95 V ou plus, charge détectée, courant de décharge disparu, ou mesure invalide | Annulation du décompte |

Ces tensions sont des choix logiciels prudents pour le pack **2S** du Tab5 ; ce ne sont pas des seuils prescrits par M5Stack. Les durées demandent également plusieurs relevés successifs. Une seule chute de tension ne suffit pas. L'affichage en pourcentage de M5Unified est une estimation issue de la tension ; il n'est pas utilisé comme déclencheur.

La réception d'un état de charge ou d'un courant positif empêche l'arrêt pendant les 30 secondes suivantes. Le système continue ainsi de fonctionner lors de la recharge et de ses variations. Une nouvelle lecture est effectuée immédiatement avant l'arrêt. Le signal de charge et le courant ne constituent toutefois pas une mesure directe de la présence de la tension USB.

Au début du décompte, l'éditeur peut demander une copie de secours asynchrone. Son résultat est indiqué : une carte absente, occupée ou défaillante peut empêcher cette sauvegarde. Le délai de protection ne promet pas qu'une écriture pourra toujours se terminer. Les mesures périodiques elles-mêmes ne provoquent aucune écriture SD ou NVS.

## Pourquoi une batterie vide peut sembler ne plus charger

M5Stack indique que le Tab5 doit être allumé et initialisé pour charger. La documentation décrit un passage en protection du pack sous 6 V. Elle donne alors la procédure : retirer la batterie, réinitialiser l'appareil, remettre la batterie, puis charger par USB-C. La phase initiale de faible courant peut durer plusieurs minutes avant le retour à la charge normale. La nouvelle protection vise à éviter d'atteindre cet état ; elle ne répare pas une batterie et ne garantit pas sa récupération.

Référence officielle : [M5Stack Tab5 — Charging](https://docs.m5stack.com/en/core/Tab5), consultée le 18 octobre 2026. Se reporter aux consignes du fabricant pour la manipulation du pack.

## Implémentation et limites vérifiables

- Cible volontairement limitée à `board_M5Tab5` : pack NP-F550 7,4 V, chargeur IP2326, INA226 à l'adresse `0x41`. Les autres cartes ne déclenchent pas d'arrêt avec ce module.
- Relevé une fois par seconde, sans allocation dynamique. Chaque transaction I2C conserve son résultat succès/échec. L'identité INA226 et le mode de conversion continue sont vérifiés. Une lecture échouée ne devient pas un faux zéro volt.
- Tension : registre INA226 `0x02`, pas de 1,25 mV. Courant : registre de shunt `0x01`, pas de 2,5 µV sur la résistance R39 de 5 mΩ. Le sens est normalisé : positif en charge, négatif en décharge. Ce calcul ne dépend pas du registre de calibration du courant.
- Charge : entrée P6 de l'expandeur `0x44`, registre `0x0f`, selon le câblage et la polarité utilisés par M5Unified. Aucun registre de commande du chargeur n'est réécrit.
- Arrêt possible seulement avec mesure valide, tension plausible de 4,5 à 8,8 V, courant de décharge d'au moins 25 mA et absence d'indication de charge active/récente. Un pack absent, des valeurs hors plage, une erreur I2C ou une interruption de relevés supérieure à 2,5 s annulent la décision.
- `M5.Power.powerOff()` émet l'impulsion `PWROFF_PLUSE` vers le superviseur matériel. M5Unified conserve un repli en sommeil profond si la coupure matérielle n'a pas eu lieu. La consommation réelle après coupure et le comportement de réveil doivent être vérifiés sur la tablette ; aucun réveil automatique par USB n'est promis.
- Ce mécanisme dépend du fonctionnement du logiciel et des capteurs. Il ne remplace pas le circuit de protection du pack, ne mesure pas chaque cellule séparément et n'est pas un étalonnage du niveau de charge.

Sources techniques primaires : [schéma Tab5, page 5](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1132/Tab5_Schematics_PDF.pdf), [INA226](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1132/INA226.pdf), [IP2326](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1132/IP2326.pdf), et code de la dépendance M5Unified 0.2.23 installée (`Power_Class.inl`, `INA226_Class.inl`, `PI4IOE5V6408_Class.inl`).
