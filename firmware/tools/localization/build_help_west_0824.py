"""Only the changed .24 help paragraphs, never filler copies of whole pages."""
from pathlib import Path
import json
HERE=Path(__file__).resolve().parent
SOURCE=json.loads((HERE/'help-segments-0824.json').read_text(encoding='utf-8'))
ROWS={
'en-US':'''In Explorer, Up/Down selects, Enter opens and Backspace goes to the parent folder. A click or tap selects; a double-click or double-tap opens. Selecting an image does not open it. Copy / Cut prepares the item; Paste acts in the current folder.
Menus group actions and their shortcuts. Sort by selects name, date or size in either direction. Right-click opens contextual actions. Open in terminal uses the current folder.
Music browses subfolders of /Audio and /mp3. The + on the left creates a subcategory. A radio category such as Jazz/Piano creates two levels. The star adds or removes a favourite. The Favourites selector filters the list; favourites remain saved on the SD card.
Volume, including zero, is retained after restarting. Media restores the tab, Favourites filter and last selection without starting playback.
Remember last view saves position and zoom when leaving Maps, without writing on every movement. With this option disabled, the home position opens. A fresh installation uses a generic view.
A USB HID or XInput controller also operates the OS: directional pad to navigate, A to confirm, B to go back. In MicroPython, the gamepad.py module provides state(). capture(True) reserves input for the script while its interface is in the foreground; capture(False) restores OS navigation.
SDR radio
Enter the frequency in kHz or MHz; the touch keypad accepts a decimal point. Ranges are suggestions and can be changed: the dongle determines accepted frequencies and inputs. AM, WFM and NFM are modes, not frequency limits.
Apply changes settings during reception; the −/+ buttons use the selected step. The spectrum has separate controls for X span and offset, Y minimum and maximum. dBFS levels are relative, without power calibration.
Set start, end, step, duration and threshold, then Scan range. Scanning makes one pass of up to 20,000 steps and can pause on a signal. Hold frequency keeps the tuned frequency. Up to 128 detections can be received again, deleted and saved to /System/SDR/detections.csv. Saving also preserves deletions. The threshold does not identify a station by name.''',
'de-DE':'''Im Dateimanager wählt Hoch/Runter ein Element, Eingabe öffnet es und Rücktaste führt zum übergeordneten Ordner. Ein Klick oder Antippen wählt aus; ein Doppelklick oder doppeltes Antippen öffnet. Das Auswählen eines Bildes öffnet es nicht. Kopieren / Ausschneiden merkt das Element vor; Einfügen wirkt im aktuellen Ordner.
Menüs bündeln Aktionen und ihre Tastenkürzel. Sortieren nach wählt Name, Datum oder Größe in beiden Richtungen. Ein Rechtsklick öffnet die Kontextaktionen. Im Terminal öffnen verwendet den aktuellen Ordner.
Musik durchsucht die Unterordner von /Audio und /mp3. Das + links erstellt eine Unterkategorie. Eine Radiokategorie wie Jazz/Piano erzeugt zwei Ebenen. Der Stern fügt einen Favoriten hinzu oder entfernt ihn. Der Favoritenwähler filtert die Liste; Favoriten bleiben auf der SD-Karte gespeichert.
Die Lautstärke, einschließlich null, bleibt nach einem Neustart erhalten. Medien stellt Registerkarte, Favoritenfilter und letzte Auswahl wieder her, ohne die Wiedergabe zu starten.
Letzte Ansicht merken speichert Position und Zoom beim Verlassen der Karte, ohne bei jeder Bewegung zu schreiben. Ist die Option deaktiviert, wird die Startposition geöffnet. Eine Neuinstallation verwendet eine allgemeine Ansicht.
Ein USB-HID- oder XInput-Controller steuert auch das OS: Steuerkreuz zum Navigieren, A zum Bestätigen, B zum Zurückgehen. In MicroPython stellt das Modul gamepad.py state() bereit. capture(True) reserviert die Eingaben für das Skript, solange dessen Oberfläche im Vordergrund ist; capture(False) gibt die OS-Navigation frei.
SDR-Radio
Frequenz in kHz oder MHz eingeben; die Touch-Tastatur erlaubt einen Dezimalpunkt. Die Bereiche sind änderbare Vorschläge: Der USB-Empfänger bestimmt die unterstützten Frequenzen und Eingänge. AM, WFM und NFM sind Betriebsarten, keine Frequenzgrenzen.
Anwenden ändert die Einstellungen während des Empfangs; die Tasten −/+ verwenden die gewählte Schrittweite. X-Breite und X-Versatz sowie Y-Minimum und Y-Maximum lassen sich getrennt einstellen. dBFS-Pegel sind relativ, ohne Leistungskalibrierung.
Anfang, Ende, Schrittweite, Dauer und Schwellwert einstellen, dann Bereich durchsuchen wählen. Der Suchlauf führt einen Durchgang mit höchstens 20.000 Schritten aus und kann bei einem Signal pausieren. Frequenz halten behält die eingestellte Frequenz bei. Bis zu 128 erkannte Signale können erneut empfangen, gelöscht und in /System/SDR/detections.csv gespeichert werden. Speichern übernimmt auch Löschungen. Der Schwellwert bestimmt keinen Sendernamen.''',
'nl-NL':'''In Bestandsbeheer selecteert Omhoog/Omlaag een item, Enter opent het en Backspace gaat naar de bovenliggende map. Eén klik of tik selecteert; dubbelklikken of dubbeltikken opent. Een afbeelding selecteren opent deze niet. Kopiëren/Knippen zet het item klaar; Plakken werkt in de huidige map.
Menu's groeperen acties en sneltoetsen. Sorteren op kiest naam, datum of grootte in beide richtingen. Rechtsklikken opent contextacties. Openen in terminal gebruikt de huidige map.
Muziek doorzoekt submappen van /Audio en /mp3. De + links maakt een subcategorie. Een radiocategorie zoals Jazz/Piano maakt twee niveaus. Het sterretje voegt een favoriet toe of verwijdert deze. De favorietenkeuze filtert de lijst; favorieten blijven op de SD-kaart bewaard.
Het volume, inclusief nul, blijft bewaard na opnieuw opstarten. Media herstelt het tabblad, het favorietenfilter en de laatste selectie zonder afspelen te starten.
Laatste weergave onthouden bewaart positie en zoom bij het verlaten van Kaarten, zonder bij elke beweging te schrijven. Uitgeschakeld wordt de startpositie geopend. Een nieuwe installatie gebruikt een algemene weergave.
Een USB HID- of XInput-controller bedient ook het OS: richtingstoetsen voor navigatie, A om te bevestigen, B om terug te gaan. In MicroPython biedt de module gamepad.py state(). capture(True) reserveert de invoer voor het script zolang de interface op de voorgrond staat; capture(False) herstelt OS-navigatie.
SDR-radio
Voer de frequentie in kHz of MHz in; het aanraaktoetsenbord accepteert een decimale punt. De bereiken zijn aanpasbare suggesties: de dongle bepaalt welke frequenties en ingangen worden geaccepteerd. AM, WFM en NFM zijn modi, geen frequentiegrenzen.
Toepassen wijzigt instellingen tijdens ontvangst; de knoppen −/+ gebruiken de gekozen stap. X-breedte en -verschuiving en Y-minimum en -maximum zijn afzonderlijk instelbaar. dBFS-niveaus zijn relatief, zonder vermogenskalibratie.
Stel begin, einde, stap, duur en drempel in en kies Bereik scannen. De scan maakt één doorgang met maximaal 20.000 stappen en kan bij een signaal pauzeren. Frequentie vasthouden behoudt de ingestelde frequentie. Maximaal 128 detecties kunnen opnieuw worden ontvangen, verwijderd en opgeslagen in /System/SDR/detections.csv. Opslaan bewaart ook verwijderingen. De drempel bepaalt geen zendernaam.''',
'es-ES':'''En el Explorador, Arriba/Abajo selecciona, Intro abre y Retroceso sube a la carpeta superior. Un clic o toque selecciona; un doble clic o doble toque abre. Seleccionar una imagen no la abre. Copiar / Cortar prepara el elemento; Pegar actúa en la carpeta actual.
Los menús agrupan las acciones y sus atajos. Ordenar por permite elegir nombre, fecha o tamaño en ambos sentidos. El clic derecho abre las acciones contextuales. Abrir en terminal utiliza la carpeta actual.
Música recorre las subcarpetas de /Audio y /mp3. El + de la izquierda crea una subcategoría. Una categoría de radio Jazz/Piano crea dos niveles. La estrella añade o elimina un favorito. El selector Favoritos filtra la lista; los favoritos se conservan en la SD.
El volumen, incluido cero, se conserva tras reiniciar. Medios recupera la pestaña, el filtro Favoritos y la última selección sin iniciar la reproducción.
Recordar la última vista guarda posición y zoom al salir de Mapas, sin escribir en cada desplazamiento. Con la opción desactivada se abre la posición inicial. Una instalación nueva utiliza una vista genérica.
Un mando USB HID o XInput también controla el sistema: cruceta para navegar, A para confirmar y B para volver. En MicroPython, el módulo gamepad.py proporciona state(). capture(True) reserva las órdenes al script mientras su interfaz está en primer plano; capture(False) devuelve la navegación al sistema.
Radio SDR
Introduzca la frecuencia en kHz o MHz; el teclado táctil acepta el punto decimal. Los intervalos son orientativos y modificables: el receptor determina las frecuencias y entradas admitidas. AM, WFM y NFM son modos, no límites de frecuencia.
Aplicar cambia los ajustes durante la recepción; las teclas −/+ utilizan el paso elegido. El espectro permite ajustar por separado anchura y desplazamiento X, mínimo y máximo Y. Los niveles dBFS son relativos, sin calibración de potencia.
Ajuste inicio, fin, paso, duración y umbral, y elija Barrido del intervalo. El barrido realiza una pasada de hasta 20.000 pasos y puede pausarse al detectar una señal. Fijar frecuencia mantiene la frecuencia sintonizada. Las 128 detecciones pueden recibirse de nuevo, eliminarse y guardarse en /System/SDR/detections.csv. Guardar también conserva las eliminaciones. El umbral no identifica el nombre de la emisora.''',
'it-IT':'''Nel Gestore file, Su/Giù selezionano, Invio apre e Backspace sale alla cartella superiore. Un clic o tocco seleziona; un doppio clic o doppio tocco apre. Selezionare un'immagine non la apre. Copia / Taglia prepara l'elemento; Incolla agisce nella cartella corrente.
I menu raggruppano azioni e scorciatoie. Ordina per sceglie nome, data o dimensione in entrambi i sensi. Il clic destro apre le azioni contestuali. Apri nel terminale usa la cartella corrente.
Musica esplora le sottocartelle di /Audio e /mp3. Il + a sinistra crea una sottocategoria. Una categoria radio Jazz/Piano crea due livelli. La stella aggiunge o rimuove un preferito. Il selettore Preferiti filtra l'elenco; i preferiti restano salvati sulla SD.
Il volume, compreso zero, viene mantenuto dopo il riavvio. Media ripristina la scheda, il filtro Preferiti e l'ultima selezione senza avviare la riproduzione.
Ricorda l'ultima vista salva posizione e zoom uscendo da Mappe, senza scrivere a ogni spostamento. Disattivando l'opzione si apre la posizione iniziale. Una nuova installazione usa una vista generica.
Un controller USB HID o XInput comanda anche il sistema: croce direzionale per navigare, A per confermare, B per tornare. In MicroPython, il modulo gamepad.py fornisce state(). capture(True) riserva i comandi allo script mentre la sua interfaccia è in primo piano; capture(False) ripristina la navigazione del sistema.
Radio SDR
Inserire la frequenza in kHz o MHz; la tastiera touch accetta il punto decimale. Gli intervalli sono indicativi e modificabili: il ricevitore determina frequenze e ingressi accettati. AM, WFM e NFM sono modalità, non limiti di frequenza.
Applica cambia le impostazioni durante la ricezione; i tasti −/+ usano il passo scelto. Lo spettro regola separatamente larghezza e scostamento X, minimo e massimo Y. I livelli dBFS sono relativi, senza calibrazione della potenza.
Impostare inizio, fine, passo, durata e soglia, poi Scansiona l'intervallo. La scansione esegue un passaggio di massimo 20.000 passi e può fermarsi su un segnale. Mantieni frequenza conserva la frequenza sintonizzata. I 128 rilevamenti possono essere ricevuti di nuovo, eliminati e salvati in /System/SDR/detections.csv. Il salvataggio conserva anche le eliminazioni. La soglia non identifica il nome della stazione.''',
'pt-BR':'''No Explorador, Cima/Baixo seleciona, Enter abre e Backspace vai à pasta superior. Um clique ou toque seleciona; um clique duplo ou toque duplo abre. Selecionar uma imagem não a abre. Copiar / Recortar prepara o item; Colar atua na pasta atual.
Os menus agrupam ações e atalhos. Ordenar por escolhe nome, data ou tamanho nos dois sentidos. O clique direito abre as ações contextuais. Abrir no terminal usa a pasta atual.
Música percorre as subpastas de /Audio e /mp3. O + à esquerda cria uma subcategoria. Uma categoria de rádio Jazz/Piano cria dois níveis. A estrela adiciona ou remove um favorito. O seletor Favoritos filtra a lista; os favoritos continuam salvos no SD.
O volume, inclusive zero, é mantido após reiniciar. Mídia restaura a aba, o filtro Favoritos e a última seleção sem iniciar a reprodução.
Lembrar a última visualização salva posição e zoom ao sair de Mapas, sem gravar a cada movimento. Com a opção desativada, abre a posição inicial. Uma instalação nova usa uma visualização genérica.
Um controle USB HID ou XInput também comanda o sistema: direcional para navegar, A para confirmar e B para voltar. No MicroPython, o módulo gamepad.py fornece state(). capture(True) reserva os comandos ao script enquanto sua interface está em primeiro plano; capture(False) restaura a navegação do sistema.
Rádio SDR
Digite a frequência em kHz ou MHz; o teclado tátil aceita o ponto decimal. As faixas são indicativas e modificáveis: o receptor determina as frequências e entradas aceitas. AM, WFM e NFM são modos, não limites de frequência.
Aplicar altera os ajustes durante a recepção; as teclas −/+ usam o passo escolhido. O espectro ajusta separadamente largura e deslocamento X, mínimo e máximo Y. Os níveis dBFS são relativos, sem calibração de potência.
Ajuste início, fim, passo, duração e limiar, e escolha Varrer a faixa. A varredura faz uma passagem de no máximo 20.000 passos e pode pausar em um sinal. Fixar frequência mantém a frequência sintonizada. As 128 detecções podem ser recebidas novamente, excluídas e salvas em /System/SDR/detections.csv. Salvar também preserva as exclusões. O limiar não identifica o nome de uma estação.''',
'pt-PT':'''No Explorador, Cima/Baixo seleciona, Enter abre e Backspace vai à pasta superior. Um clique ou toque seleciona; um duplo clique ou duplo toque abre. Selecionar uma imagem não a abre. Copiar / Cortar prepara o elemento; Colar atua na pasta atual.
Os menus agrupam ações e atalhos. Ordenar por escolhe nome, data ou tamanho nos dois sentidos. O clique direito abre as ações contextuais. Abrir no terminal utiliza a pasta atual.
Música percorre as subpastas de /Audio e /mp3. O + à esquerda cria uma subcategoria. Uma categoria de rádio Jazz/Piano cria dois níveis. A estrela adiciona ou remove um favorito. O seletor Favoritos filtra a lista; os favoritos ficam guardados no SD.
O volume, incluindo zero, é mantido após reiniciar. Multimédia repõe o separador, o filtro Favoritos e a última seleção sem iniciar a reprodução.
Memorizar a última vista guarda posição e zoom ao sair de Mapas, sem escrever a cada movimento. Com a opção desativada, abre a posição inicial. Uma instalação nova utiliza uma vista genérica.
Um comando USB HID ou XInput também controla o sistema: direcional para navegar, A para confirmar e B para voltar. No MicroPython, o módulo gamepad.py fornece state(). capture(True) reserva os comandos ao script enquanto a sua interface está em primeiro plano; capture(False) repõe a navegação do sistema.
Rádio SDR
Introduza a frequência em kHz ou MHz; o teclado tátil aceita o ponto decimal. As faixas são indicativas e modificáveis: o recetor determina as frequências e entradas aceites. AM, WFM e NFM são modos, não limites de frequência.
Aplicar altera as definições durante a receção; as teclas −/+ utilizam o passo escolhido. O espetro ajusta separadamente largura e deslocamento X, mínimo e máximo Y. Os níveis dBFS são relativos, sem calibração de potência.
Defina início, fim, passo, duração e limiar, depois Varrer a faixa. O varrimento faz uma passagem de no máximo 20 000 passos e pode suspender-se num sinal. Fixar frequência mantém a frequência sintonizada. As 128 deteções podem ser recebidas novamente, eliminadas e guardadas em /System/SDR/detections.csv. Guardar também preserva as eliminações. O limiar não identifica o nome de uma estação.'''
}
result={'fr-FR':SOURCE}
for locale,block in ROWS.items():
    rows=block.splitlines()
    assert len(rows)==len(SOURCE),(locale,len(rows))
    result[locale]=dict(zip(SOURCE,rows))
(HERE/'help-west-0824.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('Prepared 10 reviewed help segments in',len(result),'languages')
