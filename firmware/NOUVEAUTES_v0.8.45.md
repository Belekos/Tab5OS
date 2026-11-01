# Corrections 0.8.45

Base : 0.8.44 livrée, conservée intacte.

- SD : validité réelle des fichiers vérifiée avant de retenir le support ; pack cartographique fermé même si son `File` est devenu faux. Le blocage de reconnexion est reproduit et corrigé avec les sources FS/VFS exactes.
- Récupération : probe d'écriture/synchronisation/fermeture/relecture vérifié ; cause locale indépendante des erreurs concurrentes ; reprise limitée aux causes de transport déjà reconnues. Itérateurs et sélecteur libérés sans démonter un travail actif.
- SDK FS : backport d'erreurs isolé et lié au binaire vérifié, absence conservatrice, erreurs de ressources et de synchronisation préservées. SDK installé intact.
- Cartes : lieux enregistrés préservés après lecture/JSON/OOM ; mutations seulement après chargement validé.
- Zoom pendant chargement conservé, générations et propriété des tuiles vérifiées ; aucun verrou ni queue supplémentaire.
- Recherche : Internet d'abord, local après échec ou aucun résultat ; hors ligne directement sans connexion. Local disponible pendant le bail réseau radio.
- Radio : client HTTP détruit avant son transport, attente des en-têtes coopérative, URL complète retirée des traces.
- MP3/PCM : retrait des redémarrages I2S à chaque fréquence source ; rééchantillonnage natif M5Unified utilisé avec l'horloge de sortie configurée.
- Affichage : diagnostic `d` enrichi par la configuration réelle, sans nouveau timer ni modification spéculative de la dalle. Les flashs restent ouverts sur matériel.

[Audit et limites](AUDIT_0.8.45.md) · [Ressources](BILAN_RESSOURCES_0.8.45.md) · [Compilation et tests](VERIFICATION_0.8.45.md) · [Tests physiques](TESTS_TAB5_0.8.45.md).
