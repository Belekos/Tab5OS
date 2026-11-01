#pragma once
namespace tab5print {
// HTTPClient -11 is a response timeout, not a failed TLS handshake.
inline const char* transportFailure(int code, bool tls, bool established, bool printJob) {
  if (tls && !established) return "Négociation TLS de l’imprimante impossible";
  if (printJob && code == -11)
    return "Réponse tardive après envoi — vérifier la file de l’imprimante";
  if (printJob) return "Envoi interrompu — vérifier la file de l’imprimante avant de réessayer";
  return "Réponse de l’imprimante absente ou interrompue";
}
inline unsigned responseTimeout(bool printJob) { return printJob ? 60000 : 15000; }
}
