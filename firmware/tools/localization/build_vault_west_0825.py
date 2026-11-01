from pathlib import Path
import json
HERE=Path(__file__).resolve().parent
KEYS=json.loads((HERE/'vault-ui-keys-0825.json').read_text(encoding='utf8'))
ROWS={
'en-US':'''Delete key
On this Tab5 (fast)
Portable (password)
No saved credentials
Confirm deletion
PIN: 4 to 12 digits
PINs do not match
The vault protects files and credentials. A PIN can restrict access on this Tab5.
Vault PIN
Optional PIN · 4 to 12 digits
Confirm PIN
Save PIN
Remove PIN
The PIN protects access on the tablet. A portable backup lets you recover keys on another device.
Old vault · password
Back to vault
Password vault
Copy the keys to the device vault to use the password manager.
Saved credentials
Variable name · letters, digits and _
Username, password or token
Show / hide
Copy · 30 s
Available by name in the terminal, Python and applications. Locking the vault blocks access to protected values.
No key selected
Without a backup of this key, its encrypted files cannot be opened again.
Credential saved
Credential saved · environment full
Credential deleted
Copied for 30 seconds
Portable copy: password of at least 12 characters. Key derivation takes time; Cancel remains available.
Fast copy: restore only on this Tab5. To move to another device, choose a portable copy.''',
'de-DE':'''Schlüssel löschen
Auf diesem Tab5 (schnell)
Übertragbar (Passwort)
Keine Zugangsdaten gespeichert
Löschen bestätigen
PIN: 4 bis 12 Ziffern
Die PINs stimmen nicht überein
Der Tresor schützt Dateien und Zugangsdaten. Eine PIN kann den Zugriff auf diesem Tab5 beschränken.
Tresor-PIN
Optionale PIN · 4 bis 12 Ziffern
PIN bestätigen
PIN speichern
PIN entfernen
Die PIN schützt den Zugriff auf dem Tablet. Eine übertragbare Sicherung ermöglicht die Wiederherstellung der Schlüssel auf einem anderen Gerät.
Alter Tresor · Passwort
Zurück zum Tresor
Passworttresor
Kopieren Sie die Schlüssel in den Gerätetresor, um die Passwortverwaltung zu verwenden.
Gespeicherte Zugangsdaten
Variablenname · Buchstaben, Ziffern und _
Benutzername, Passwort oder Token
Anzeigen / ausblenden
Kopieren · 30 s
Über den Namen im Terminal, in Python und Anwendungen verfügbar. Das Sperren des Tresors unterbindet den Zugriff auf geschützte Werte.
Kein Schlüssel ausgewählt
Ohne Sicherung dieses Schlüssels können die damit verschlüsselten Dateien nicht mehr geöffnet werden.
Zugangsdaten gespeichert
Zugangsdaten gespeichert · Umgebung voll
Zugangsdaten gelöscht
Für 30 Sekunden kopiert
Übertragbare Kopie: Passwort mit mindestens 12 Zeichen. Die Schlüsselableitung benötigt Zeit; Abbrechen bleibt verfügbar.
Schnelle Kopie: Wiederherstellung nur auf diesem Tab5. Für einen Gerätewechsel eine übertragbare Kopie wählen.''',
'es-ES':'''Eliminar la clave
En este Tab5 (rápido)
Portátil (contraseña)
No hay credenciales guardadas
Confirmar eliminación
PIN: de 4 a 12 dígitos
Los PIN no coinciden
La caja fuerte protege archivos y credenciales. Un PIN puede limitar el acceso en este Tab5.
PIN de la caja fuerte
PIN opcional · de 4 a 12 dígitos
Confirmar el PIN
Guardar el PIN
Quitar el PIN
El PIN protege el acceso en la tableta. Una copia portátil permite recuperar las claves en otro dispositivo.
Caja fuerte antigua · contraseña
Volver a la caja fuerte
Caja fuerte de contraseñas
Copia las claves a la caja fuerte del dispositivo para usar el gestor de contraseñas.
Credenciales guardadas
Nombre de variable · letras, dígitos y _
Usuario, contraseña o token
Mostrar / ocultar
Copiar · 30 s
Disponible por su nombre en el terminal, Python y las aplicaciones. Bloquear la caja fuerte impide acceder a los valores protegidos.
No hay ninguna clave seleccionada
Sin una copia de esta clave, sus archivos cifrados no se podrán volver a abrir.
Credencial guardada
Credencial guardada · entorno lleno
Credencial eliminada
Copiado durante 30 segundos
Copia portátil: contraseña de al menos 12 caracteres. La derivación de la clave tarda; Cancelar sigue disponible.
Copia rápida: solo se restaura en este Tab5. Para cambiar de dispositivo, elige una copia portátil.''',
'it-IT':'''Elimina la chiave
Su questo Tab5 (rapido)
Portabile (password)
Nessuna credenziale salvata
Conferma eliminazione
PIN: da 4 a 12 cifre
I PIN non corrispondono
La cassaforte protegge file e credenziali. Un PIN può limitarne l'accesso su questo Tab5.
PIN della cassaforte
PIN facoltativo · da 4 a 12 cifre
Conferma il PIN
Salva il PIN
Rimuovi il PIN
Il PIN protegge l'accesso sul tablet. Una copia portabile consente di recuperare le chiavi su un altro dispositivo.
Vecchia cassaforte · password
Torna alla cassaforte
Cassaforte delle password
Copia le chiavi nella cassaforte del dispositivo per usare il gestore delle password.
Credenziali salvate
Nome della variabile · lettere, cifre e _
Nome utente, password o token
Mostra / nascondi
Copia · 30 s
Accessibile per nome nel terminale, in Python e nelle applicazioni. Bloccare la cassaforte impedisce l'accesso ai valori protetti.
Nessuna chiave selezionata
Senza una copia di questa chiave, i file cifrati con essa non potranno più essere aperti.
Credenziale salvata
Credenziale salvata · ambiente pieno
Credenziale eliminata
Copiato per 30 secondi
Copia portabile: password di almeno 12 caratteri. La derivazione della chiave richiede tempo; Annulla resta disponibile.
Copia rapida: ripristino solo su questo Tab5. Per cambiare dispositivo, scegli una copia portabile.''',
'nl-NL':'''Sleutel verwijderen
Op deze Tab5 (snel)
Overdraagbaar (wachtwoord)
Geen opgeslagen aanmeldgegevens
Verwijderen bevestigen
PIN: 4 tot 12 cijfers
De pincodes komen niet overeen
De kluis beschermt bestanden en aanmeldgegevens. Een pincode kan toegang op deze Tab5 beperken.
Pincode van de kluis
Optionele pincode · 4 tot 12 cijfers
Pincode bevestigen
Pincode opslaan
Pincode verwijderen
De pincode beschermt toegang op de tablet. Met een overdraagbare back-up kunt u sleutels op een ander apparaat herstellen.
Oude kluis · wachtwoord
Terug naar de kluis
Wachtwoordkluis
Kopieer de sleutels naar de apparaatkluis om de wachtwoordbeheerder te gebruiken.
Opgeslagen aanmeldgegevens
Variabelenaam · letters, cijfers en _
Gebruikersnaam, wachtwoord of token
Tonen / verbergen
Kopiëren · 30 s
Via de naam beschikbaar in de terminal, Python en toepassingen. Door de kluis te vergrendelen wordt toegang tot beveiligde waarden geblokkeerd.
Geen sleutel geselecteerd
Zonder back-up van deze sleutel kunnen de ermee versleutelde bestanden niet meer worden geopend.
Aanmeldgegeven opgeslagen
Aanmeldgegeven opgeslagen · omgeving vol
Aanmeldgegeven verwijderd
Voor 30 seconden gekopieerd
Overdraagbare kopie: wachtwoord van minstens 12 tekens. Sleutelafleiding kost tijd; Annuleren blijft beschikbaar.
Snelle kopie: alleen op deze Tab5 te herstellen. Kies een overdraagbare kopie voor een ander apparaat.''',
'pt-BR':'''Excluir a chave
Neste Tab5 (rápido)
Portátil (senha)
Nenhuma credencial salva
Confirmar exclusão
PIN: de 4 a 12 dígitos
Os PINs não coincidem
O cofre protege arquivos e credenciais. Um PIN pode limitar o acesso neste Tab5.
PIN do cofre
PIN opcional · de 4 a 12 dígitos
Confirmar o PIN
Salvar o PIN
Remover o PIN
O PIN protege o acesso no tablet. Uma cópia portátil permite recuperar as chaves em outro dispositivo.
Cofre antigo · senha
Voltar ao cofre
Cofre de senhas
Copie as chaves para o cofre do dispositivo para usar o gerenciador de senhas.
Credenciais salvas
Nome da variável · letras, dígitos e _
Usuário, senha ou token
Mostrar / ocultar
Copiar · 30 s
Acessível pelo nome no terminal, Python e aplicativos. Bloquear o cofre impede o acesso aos valores protegidos.
Nenhuma chave selecionada
Sem uma cópia desta chave, os arquivos criptografados com ela não poderão ser abertos novamente.
Credencial salva
Credencial salva · ambiente cheio
Credencial excluída
Copiado por 30 segundos
Cópia portátil: senha de pelo menos 12 caracteres. A derivação da chave demora; Cancelar continua disponível.
Cópia rápida: restauração apenas neste Tab5. Para mudar de dispositivo, escolha uma cópia portátil.''',
'pt-PT':'''Eliminar a chave
Neste Tab5 (rápido)
Portátil (palavra-passe)
Nenhuma credencial guardada
Confirmar eliminação
PIN: de 4 a 12 dígitos
Os PIN não coincidem
O cofre protege ficheiros e credenciais. Um PIN pode limitar o acesso neste Tab5.
PIN do cofre
PIN opcional · de 4 a 12 dígitos
Confirmar o PIN
Guardar o PIN
Remover o PIN
O PIN protege o acesso no tablet. Uma cópia portátil permite recuperar as chaves noutro dispositivo.
Cofre antigo · palavra-passe
Voltar ao cofre
Cofre de palavras-passe
Copie as chaves para o cofre do dispositivo para usar o gestor de palavras-passe.
Credenciais guardadas
Nome da variável · letras, dígitos e _
Utilizador, palavra-passe ou token
Mostrar / ocultar
Copiar · 30 s
Acessível pelo nome no terminal, Python e aplicações. Bloquear o cofre impede o acesso aos valores protegidos.
Nenhuma chave selecionada
Sem uma cópia desta chave, os ficheiros cifrados com ela não poderão voltar a ser abertos.
Credencial guardada
Credencial guardada · ambiente cheio
Credencial eliminada
Copiado durante 30 segundos
Cópia portátil: palavra-passe de pelo menos 12 caracteres. A derivação da chave demora; Cancelar continua disponível.
Cópia rápida: restauro apenas neste Tab5. Para mudar de dispositivo, escolha uma cópia portátil.'''}
assert len(KEYS)==32
delta={'fr-FR':dict(zip(KEYS,KEYS))}
for locale,text in ROWS.items():
    values=text.splitlines();assert len(values)==len(KEYS),(locale,len(values),len(KEYS))
    delta[locale]=dict(zip(KEYS,values))
(HERE/'vault-ui-west-0825.json').write_text(json.dumps(delta,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('Prepared',len(KEYS),'vault UI keys in',len(delta),'western/source languages')
