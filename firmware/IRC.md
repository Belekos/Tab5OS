# Discussion IRC

Utiliser le firmware **0.8.19** et les modules SD correspondants. Depuis l’accueil, ouvrir **Applications Python → Discussion IRC**.

Choisir un serveur, saisir son pseudo et, si souhaité, un salon commençant par `#`. **Se connecter** ouvre la connexion ; l'application ne se connecte jamais toute seule. Le dernier serveur et le pseudo restent mémorisés. **Options** permet de saisir un serveur personnalisé, modifier le pseudo enregistré ou effacer les récents. Le pseudo est enregistré localement ; cela ne crée pas de compte NickServ sur le réseau.

Les serveurs proposés utilisent les points d'entrée TLS publiés par leurs opérateurs :

| Réseau | Serveur | Port TLS |
|---|---|---|
| Libera Chat | `irc.libera.chat` | 6697 |
| OFTC | `irc.oftc.net` | 6697 |
| hackint | `irc.hackint.org` | 6697 |

Libera est celui validé sur la tablette par l'utilisateur. Les deux autres adresses ont été vérifiées dans leur documentation officielle ; leur accès dépend du réseau et de leurs règles. La date de la tablette doit être correcte pour vérifier les certificats. Les huit derniers serveurs utilisés sont proposés dans la liste ; choisir un serveur ne lance pas la connexion.

Le bouton **Salons** ouvre la gestion des salons pendant la connexion :

- **Actifs**, **Favoris** ou **Récents** affichent les salons du serveur courant. Choisir un nom remplit le champ sans rejoindre automatiquement le salon.
- **Rejoindre** ouvre le salon saisi et revient à la conversation.
- **Ajouter aux favoris** et **Retirer des favoris** modifient les favoris sans rejoindre ni quitter un salon.
- **Quitter le salon** envoie PART et ferme cette conversation. Son favori éventuel reste enregistré.

Les seize favoris et seize salons récents sont associés au serveur et au port. Une connexion accepte jusqu'à huit conversations simultanées. Les salons entrent dans les récents quand le serveur confirme leur ouverture. Les favoris ne sont jamais rejoints automatiquement. PING/PONG continue dans la page Salons et lorsque l'application est masquée.

Saisir un texte puis **Entrée** sur le clavier USB ou tactile, ou **Envoyer**. L'édition reste ouverte pour le message suivant. La liste supérieure change de conversation ; **Serveur** affiche les réponses et erreurs IRC. Les flèches remontent l'historique ou reviennent aux messages récents.

| Commande | Action |
|---|---|
| `/join #salon` | Rejoindre un salon |
| `/msg pseudo texte` | Message privé |
| `/me texte` | Action dans la conversation active |
| `/nick nouveau_pseudo` | Changer de pseudo ; le pseudo confirmé par le serveur sera mémorisé |
| `/part` | Quitter et fermer le salon actif |
| `/close` | Fermer la conversation, effacer son historique et quitter le salon le cas échéant |
| `/quit` | Déconnecter |
| `/help` | Rappeler les commandes |

**Masquer** conserve la connexion ; reprendre l'app avec son icône ou **Ctrl+Alt+P**. **Arrêter**, ou **Ctrl+.**, termine le script. Une seule application Python fonctionne à la fois ; les applications natives peuvent continuer selon les ressources disponibles.

Les préférences sont stockées dans `/System/irc.json`, avec remplacement journalisé sur SD. Les changements reçus du serveur sont regroupés avant écriture ; une erreur SD laisse la conversation fonctionner et affiche l'échec de sauvegarde. Après remise en état de la SD, **Options → Enregistrer** permet de réessayer. Les messages ne sont pas enregistrés. L'historique total en mémoire reste limité à 128 lignes de protocole ; les longues lignes peuvent occuper plusieurs lignes d'affichage. La file d'envoi comporte huit messages, espacés d'au moins 500 ms ; l'affichage d'un message envoyé n'est pas un accusé de lecture. Les réponses PONG passent avant cette file.

La connexion reste exclusivement **TLS avec certificats vérifiés**. Cette version ne gère pas l'authentification SASL automatique, les mots de passe serveur, DCC ou les transferts de fichiers. Un réseau ou un salon peut imposer un compte authentifié. Il n'y a pas de reconnexion automatique après coupure.

Sources vérifiées le 27 septembre 2026 : [Libera.Chat](https://libera.chat/guides/connect), [OFTC](https://www.oftc.net/) et [hackint](https://www.hackint.org/connect).
