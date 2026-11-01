# Network diagnostics tests

`run_msvc.cmd <sketch-directory>` runs the production protocol helper and a
byte-for-byte copy of `Tab5NetworkTools.cpp` with deterministic simulated
platform calls. Alternatively use `python run.py <sketch-directory>` from an
MSVC development shell. Nothing contacts the network.

Coverage: invalid names/ports, truncated or unrelated ICMP packets, checksums,
echo nonce/identifier/sequence, IPv4 options, fragments, Wi-Fi/network gate,
four-request limit, socket cleanup, TCP refusal/timeout/cancellation, DNS cache
hit/error, DNS scheduling failure, pending-request cap and callback after timeout
or cancellation. The copied production source hash is printed.

`run_syntax.py --database compile_commands.json --sketch <directory> --output report.json`
reuses an existing ESP32-P4 compile command to check the network tools, terminal
and shell. This checks target API compatibility but does not emulate Wi-Fi.

The firmware build and physical Tab5 exercise remain separate validation gates.
# Terminal et réseau — 0.8.19

Exécuter `python verify_terminal_0819.py` depuis Windows avec Visual Studio 2022
Community. Le rapport `tools/verification/terminal-network-0819.json` contient
les résultats et empreintes des sources réellement testées.

- `protocol.cpp`, `session.cpp` : validation, ICMP, DNS asynchrone, arrêt,
  nettoyage des sockets, options usuelles, délais fractionnaires et scans bornés.
- `arguments.cpp` : erreurs de paramètres, options HTTP, nom distant `curl -O`,
  délais longs et résolution des chemins SD/USB.
- `http.cpp` : vrai code du téléchargement avec transport simulé. Vérifie les
  fichiers temporaires, l'ancienne destination en cas d'échec/annulation, les
  écritures partielles, redirections et délais. La cryptographie TLS réelle
  reste à vérifier sur l'appareil ; le client de production utilise les CA du SDK.
- `shell.cpp` : vrai moteur de commandes avec adaptateurs POSIX Windows.
  Vérifie aide, métadonnées, arborescence, comparaison, inspection hexadécimale,
  variables, redirection, protection des racines et annulation d'une copie.

Les outils n'utilisent pas de machine ni de cible réseau extérieure durant ces
tests. Les fixtures ne constituent pas un essai matériel du réseau.

