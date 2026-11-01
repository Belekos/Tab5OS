# Copie de secours de l’éditeur — 0.8.28

Au début du décompte d’arrêt pour batterie faible, l’OS demande une copie du document modifié. Il s’agit d’une tentative limitée par le délai restant : une carte SD absente ou défaillante peut empêcher la sauvegarde.

La copie est enregistrée dans `/System/writing/recovery.md`, ou `recovery.py` pour le mode code. Lorsque le document ouvert est lui-même ce fichier, la destination devient `recovery-copy.md` ou `recovery-copy.py`, afin de conserver son original. Le texte est copié exactement ; les images liées ne sont pas dupliquées. Les fichiers de récupération sont accessibles depuis l’explorateur.

La copie utilise la transaction de sauvegarde existante : fichier temporaire, relecture complète, vérification du contenu, conservation de la récupération précédente dans le répertoire de sauvegardes, puis remplacement. La préparation prend un instantané du document en PSRAM, limité à 2 Mio ; chaque passage de service écrit ou vérifie au plus 4 Kio. Une sauvegarde ou restauration déjà engagée termine avant le début de cette copie.

Le chemin du document ouvert, son historique d’annulation et son état modifié restent inchangés. Le contenu saisi après l’instantané ne fait pas partie de la copie. Si une sauvegarde normale déjà en cours a enregistré le document, aucune copie supplémentaire n’est écrite. Un échec n’entraîne aucune nouvelle tentative automatique.

À l’échéance, une copie encore incomplète est interrompue : le temporaire est fermé sans remplacer le fichier de récupération existant. L’OS n’attend pas indéfiniment la fin d’un document volumineux avant la coupure de protection. Cela ne garantit toutefois pas un temps maximal pour une opération individuelle d’une carte SD défectueuse.

API interne : `requestRecoverySave()` déclenche une tentative ; `recoverySaveState()` renvoie `Idle`, `Pending`, `Saved` ou `Failed`. `recoverySavePath()` et `recoverySaveError()` donnent le résultat. `cancelRecoverySave()` termine une tentative encore en cours. L’OS doit appeler la demande une seule fois par décompte et laisser fonctionner le service ordinaire de l’éditeur.

La suite native exécute les contrôleurs réels avec `Document` et `SaveJob`, sous AddressSanitizer. Elle couvre un document de 2 Mio, les échecs de création/écriture/relecture/vérification/renommage, le retour à la récupération précédente après échec du remplacement final, quatre moments d’interruption, les opérations déjà engagées et la conservation du document. Ces essais utilisent un stockage simulé ; aucune mesure d’arrêt sur batterie réelle n’a été réalisée.
