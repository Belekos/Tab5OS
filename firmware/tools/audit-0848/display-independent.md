# Revue indépendante du chemin DSI pendant une indisponibilité du cache Flash

La revue porte sur les sources SDK 5.5.4 conservées avec empreintes, leur préparation isolée, le hook `Tab5DisplaySignals` et les instructions réellement liées. Placer les fonctions en IRAM ne suffit pas : les interruptions doivent aussi être allouées avec `ESP_INTR_FLAG_IRAM`, et le contexte, les métadonnées DMA et les chaînes utilisées par ces fonctions doivent rester accessibles.

`tools/build_linux/verify_dsi_cache_safe.py` vérifie 48 conditions dans l'ELF final :

- DSI : flags passés réellement à l'allocateur 1038 (LOWMED + IRAM), via le wrapper d'observation.
- DW-GDMA : valeur par défaut 1294 (SHARED + LOWMED + IRAM), et branche de priorité explicite qui conserve SHARED + IRAM (1280).
- Capacités des allocations de contexte bus DSI (28 octets), panneau DPI (136), canal DW (44), groupe DW (36), liste DW (12) ; ces contextes doivent utiliser INTERNAL + 8BIT. Les descripteurs de liste conservent INTERNAL + DMA.
- Placement IRAM du graphe direct suivi, références aux données qui ne pointent pas en Flash, dépendance forte au hook de sous-alimentation et absence d'IO dans ce hook. Les fonctions ROM autorisées sont identifiées par leur symbole et adresse exacte du SDK cible.

Le hook ne fait que lire le compteur temporel natif et mettre à jour des atomiques en mémoire interne. La journalisation a lieu ensuite dans la tâche UI ; le pilote conserve la propriété de l'acquittement des interruptions et de la relance DMA. Les rares guards d'erreur DW gardent leurs retours d'erreur et leur diagnostic natif, avec littéraux en DRAM.

Le vérificateur rejette le véritable ELF livré de .47 sur quinze conditions attendues : flags IRQ sans IRAM, trois contextes DW alloués en DEFAULT, données Flash sur le chemin d'interruption, et absence du nouveau hook. Les contextes bus/panneau étaient déjà internes : ils ne sont pas présentés comme un gain nouveau. La fonction temporelle native utilise un appel HAL en ROM qui reste accessible.

Le rapport positif .48 doit être produit à partir de l'ELF effectivement compilé, et non d'une commande de compilation ou de macros ajoutées à des objets déjà compilés. Le rapport négatif `dsi-cache-baseline-0847.json` est un témoin comparatif, pas un échec des sources corrigées.

Ces contrôles statiques ne mesurent ni les flashs visibles ni les délais d'accès PSRAM. Les pointeurs de callbacks indirects restent soumis aux validations natives IRAM/contexte lors de l'enregistrement ; cela ne constitue pas une isolation mémoire globale du firmware ou des applications ELF.
