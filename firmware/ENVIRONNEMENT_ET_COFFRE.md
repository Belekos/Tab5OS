# Environnement partagé et coffre — 0.8.28

Le coffre conserve des entrées nommées chiffrées. Son premier accès impose un PIN ; modifier ce PIN exige le PIN actuel. Une entrée devient une variable protégée après déverrouillage. Sa valeur est relue à chaque demande ; verrouiller le coffre bloque les nouvelles lectures, y compris depuis un terminal déjà ouvert. La suppression de l'entrée rend sa référence inutilisable : elle ne revient jamais à une ancienne valeur publique homonyme.

L'environnement public vit uniquement en RAM pendant le démarrage courant et est partagé entre les applications. Fermer le terminal ou terminer un script Python ne le détruit pas. Les variables locales `set NOM valeur` restent propres au terminal ; `export NOM=valeur` rend une valeur publique commune. Les références protégées conservent uniquement le nom de l'entrée, jamais une copie du mot de passe. Il existe au maximum 64 variables communes, avec des noms ASCII de 48 caractères et des valeurs de 512 octets au maximum.

## Shell

```sh
export LANG=en-GB
export --secret API_TOKEN MON_JETON
env
# env affiche uniquement les noms, sans les valeurs.
# Expansion normale, avec guillemets si la valeur contient des espaces :
commande "$API_TOKEN"
unset API_TOKEN
```

`export --secret NOM [ENTREE]` crée une référence, sans lire le secret. Sans second nom, l'entrée porte le même nom que la variable. `unset` retire la liaison en RAM, sans effacer le coffre ; le prochain déverrouillage réimporte les noms des entrées restantes. Un export public ne peut pas écraser une référence protégée : il faut d'abord retirer explicitement cette liaison.

Les affectations et les commandes `set` / `export` ne sont ni affichées comme commande ni ajoutées à l'historique du terminal. L'historique ne contient jamais les expansions automatiques. Un programme ou un `echo "$API_TOKEN"` demandé explicitement peut néanmoins afficher la valeur. Le système ne prétend pas isoler les programmes : un programme auquel on donne un secret peut le copier, le conserver, le transmettre ou l'afficher. Une copie déjà obtenue par un programme n'est pas révoquée par le verrouillage. Les scripts présents sur la SD ne doivent pas contenir de secrets en clair.

L'explorateur lance les `.sh` dans le terminal natif en plaçant le dossier courant sur leur dossier parent. Les chemins contenant des espaces ou des apostrophes sont protégés ; les supports SD et USB sont acceptés. Les scripts conservent la grammaire du terminal Tab5, avec une limite de 32 Kio et des commandes exécutées successivement.

## MicroPython

```python
import tab5
tab5.setenv("LANG", "en-GB")       # valeur publique en RAM
tab5.bindenv("API_TOKEN", "MON_JETON")
token = tab5.env("API_TOKEN")     # OSError si coffre verrouillé ou entrée absente
noms = tab5.env_names()           # noms seulement
tab5.unsetenv("API_TOKEN")
```

`tab5.env("INCONNUE", valeur_par_defaut)` utilise la valeur par défaut uniquement si aucune liaison n'existe. Il ne masque jamais l'erreur de verrouillage d'une référence protégée. Cette interface appartient au module `tab5` livré ; elle ne modifie pas `os.getenv` ou l'environnement libc.

## Applications natives et ELF

Le code natif inclut `Tab5Environment.h`. `tab5_env_get(nom, tampon, capacité)` copie la valeur et renvoie sa longueur, ou un errno négatif ; il efface le tampon fourni en cas d'échec. `tab5_env_set`, `tab5_env_bind` et `tab5_env_unset` utilisent les mêmes règles que le shell. Les valeurs sont copiées, aucun pointeur interne n'est exposé.

Les ELF ABI 3, 4 et 5 utilisent leur callback `hardware` existant. Le format est identique à celui de MicroPython :

| Opération | a | Entrée | Sortie |
|---|---:|---|---|
| 107, lire | 0 | `NOM\0` | valeur UTF-8, longueur sans NUL |
| 108, public | 0 | `NOM\0VALEUR\0` | 0 ou errno négatif |
| 108, référence | 1 | `NOM\0ENTREE\0` | 0 ou errno négatif |
| 109, retirer | 0 | `NOM\0` | 0 ou errno négatif |
| 109, noms | 1 | aucune | noms séparés par `\n` |

```c
char token[513];
int n = api3->hardware(107, 0, 0, 0, "API_TOKEN", 10,
                      token, sizeof(token));
/* Utiliser seulement si n >= 0, puis effacer token après usage. */
```

Les ABI 1 et 2 restent compatibles mais n'exposent pas ce broker. Les applications ELF sont du code de confiance partageant la mémoire du système ; le coffre n'est pas un bac à sable.

## Git

Créer deux entrées dans le coffre, par exemple `GIT_USER` et `GIT_TOKEN`, puis les choisir dans **Identité et accès Git**, ou dans les deux sélecteurs du formulaire de clonage. L'option **Aucun** retire une référence. Pour utiliser à nouveau un ancien fichier d'identification, retirer les deux références au coffre. Ce fichier reste compatible ; sa protection est celle du fichier existant.

```sh
git config tab5.usernameSecret GIT_USER
git config tab5.tokenSecret GIT_TOKEN
git fetch
# Clonage d’un dépôt privé :
git clone --username-secret GIT_USER --token-secret GIT_TOKEN https://example.org/projet.git projet
# Retrait des références :
git config --unset tab5.usernameSecret
git config --unset tab5.tokenSecret
```

Seuls les noms sont enregistrés dans `.git/config`. Les identifiants sont résolus au moment où le serveur les demande, uniquement pour l'origine HTTPS attendue. Une référence incomplète, supprimée ou verrouillée fait échouer l'authentification sans repli vers le fichier d'identification. Les valeurs ne sont placées ni dans l'URL ni dans les états Git. Le transfert déjà authentifié peut continuer après un verrouillage : les données déjà confiées à la connexion ne sont pas rappelées.

Les tests hôtes exécutent les vrais parseur, environnement, callback d'authentification et commandes de configuration Git. Ils simulent la frontière du coffre ; la cryptographie, les interruptions SD et la tablette nécessitent leurs validations propres.

## Chargement des identifiants en 0.8.28

Les identifiants ne deviennent disponibles qu’après le chargement réussi de leur banque. Un échec de lecture reste mémorisé pendant la session ; les appels de l’interface ne provoquent pas une boucle de lecture/déchiffrement. **Réessayer** déclenche une nouvelle tentative explicite. Les variables secrètes restent indisponibles si le coffre ou la banque d’identifiants n’est pas ouvert. Voir [COFFRE_0827.md](COFFRE_0827.md).
