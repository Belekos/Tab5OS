"""Apply reviewed .24 translations. Missing languages/rows are never padded."""
from pathlib import Path
import json

HERE = Path(__file__).resolve().parent
RELEASE = HERE.parents[1]
SKETCH = next(RELEASE.glob('Tab5OS_Arduino_*_Ecriture'))
KEYS = json.loads((HERE/'reference-delta-0824.json').read_text(encoding='utf-8'))
ROWS = {
'en-US': '''Sort by
Name: A to Z
Name: Z to A
Date: oldest first
Date: newest first
Size: smallest first
Size: largest first
Last view not saved
Select Open to display this image.
Loading.
Not enough memory - use search
Map settings
Remember last view
Position and zoom are saved when leaving Maps. When disabled: open the home view.
Pending detections: scan paused
Frequency or input rejected by the dongle
Sample rate rejected by the dongle
Scanning
Scan complete
Spectrum active: audio output busy
Signal detected: listening with scan paused
Invalid frequency or mode
Searching for tuner
Stopping
Invalid or excessively wide scan
Scan paused: hold frequency
Custom
Longwave
Mediumwave
Shortwave
FM 64–108 MHz
FM 87.5–108 MHz
Aviation VHF
VHF 144–148 MHz
Marine VHF
UHF 430–440 MHz
UHF 446 MHz
UHF 863–870 MHz
Invalid frequency
No detections
Detections
Invalid detections file
Detections saved to /System/SDR/detections.csv
Invalid frequency step
Choose AM, WFM or NFM to listen
Invalid axes: minimum must be below maximum
List full: delete some detections
Frequency
Unit
Step (kHz)
Demodulation
Spectrum
Receive / apply
Scan
X span
X offset (kHz)
Y min (dBFS)
Y max (dBFS)
Apply axes
Store frequency
Hold frequency
Suggested range
Tuner
Direct I
Direct Q
Ranges are suggestions and can be changed. The dongle determines accepted frequencies. Direct I/Q requires a suitable hardware input.
Start (kHz)
End (kHz)
Time per step (ms)
Threshold (dBFS)
Pause scanning on a signal
Scan range
One pass, up to 20,000 steps. The threshold detects a signal without identifying the station. Set the step above.
Receive selection
Clear list
Save list
Up to 128 detections. Selection alone does not change reception. Saving also preserves deletions.
Reports received / rejected
USB errors / restarts
Filter media
Selection saved
Not enough memory for the media list''',
'de-DE': '''Sortieren nach
Name: A bis Z
Name: Z bis A
Datum: älteste zuerst
Datum: neueste zuerst
Größe: aufsteigend
Größe: absteigend
Letzte Ansicht nicht gespeichert
Öffnen wählen, um dieses Bild anzuzeigen.
Wird geladen.
Nicht genug Speicher - Suche verwenden
Karteneinstellungen
Letzte Ansicht merken
Position und Zoom werden beim Verlassen der Karte gespeichert. Deaktiviert: Startansicht öffnen.
Ausstehende Erkennungen: Suchlauf pausiert
Frequenz oder Eingang vom USB-Empfänger abgelehnt
Abtastrate vom USB-Empfänger abgelehnt
Suchlauf läuft
Suchlauf abgeschlossen
Spektrum aktiv: Audioausgabe belegt
Signal erkannt: Empfang bei pausiertem Suchlauf
Ungültige Frequenz oder Betriebsart
Tuner wird gesucht
Wird angehalten
Ungültiger oder zu großer Suchbereich
Suchlauf pausiert: feste Frequenz
Frei wählbar
Langwelle
Mittelwelle
Kurzwelle
FM 64–108 MHz
FM 87,5–108 MHz
VHF-Flugfunk
VHF 144–148 MHz
VHF-Seefunk
UHF 430–440 MHz
UHF 446 MHz
UHF 863–870 MHz
Ungültige Frequenz
Keine Erkennungen
Erkannte Signale
Ungültige Datei mit erkannten Signalen
Erkannte Signale in /System/SDR/detections.csv gespeichert
Ungültige Frequenzschrittweite
Zum Hören AM, WFM oder NFM wählen
Ungültige Achsen: Minimum muss kleiner als Maximum sein
Liste voll: erkannte Signale löschen
Frequenz
Einheit
Schrittweite (kHz)
Demodulation
Spektrum
Empfangen / anwenden
Suchlauf
X-Breite
X-Versatz (kHz)
Y min (dBFS)
Y max (dBFS)
Achsen anwenden
Frequenz merken
Frequenz halten
Vorgeschlagener Bereich
Tuner
Direkt I
Direkt Q
Die Bereiche sind änderbare Vorschläge. Der USB-Empfänger bestimmt die zulässigen Frequenzen. Direkt I/Q benötigt einen geeigneten Hardwareeingang.
Anfang (kHz)
Ende (kHz)
Dauer pro Schritt (ms)
Schwellwert (dBFS)
Suchlauf bei einem Signal pausieren
Bereich durchsuchen
Ein Durchlauf mit höchstens 20.000 Schritten. Der Schwellwert erkennt ein Signal, identifiziert aber nicht den Sender. Die Schrittweite wird oben eingestellt.
Auswahl empfangen
Liste leeren
Liste speichern
Höchstens 128 erkannte Signale. Die Auswahl allein ändert den Empfang nicht. Beim Speichern werden auch Löschungen übernommen.
Berichte empfangen / verworfen
USB-Fehler / Neustarts
Medien filtern
Auswahl gespeichert
Nicht genug Speicher für die Medienliste''',
'nl-NL': '''Sorteren op
Naam: A tot Z
Naam: Z tot A
Datum: oudste eerst
Datum: nieuwste eerst
Grootte: oplopend
Grootte: aflopend
Laatste weergave niet opgeslagen
Kies Openen om deze afbeelding te tonen.
Laden.
Onvoldoende geheugen - gebruik zoeken
Kaartinstellingen
Laatste weergave onthouden
Positie en zoom worden opgeslagen bij het verlaten van Kaarten. Uitgeschakeld: de startweergave openen.
Detecties in de wachtrij: scan gepauzeerd
Frequentie of ingang geweigerd door de dongle
Bemonsteringsfrequentie geweigerd door de dongle
Scannen
Scan voltooid
Spectrum actief: audio-uitgang bezet
Signaal gedetecteerd: luisteren met gepauzeerde scan
Ongeldige frequentie of modus
Tuner zoeken
Stoppen
Ongeldige of te uitgebreide scan
Scan gepauzeerd: vaste frequentie
Vrij instelbaar
Lange golf
Middengolf
Korte golf
FM 64–108 MHz
FM 87,5–108 MHz
Luchtvaart-VHF
VHF 144–148 MHz
Maritieme VHF
UHF 430–440 MHz
UHF 446 MHz
UHF 863–870 MHz
Ongeldige frequentie
Geen detecties
Detecties
Ongeldig detectiebestand
Detecties opgeslagen in /System/SDR/detections.csv
Ongeldige frequentiestap
Kies AM, WFM of NFM om te luisteren
Ongeldige assen: minimum moet lager zijn dan maximum
Lijst vol: verwijder detecties
Frequentie
Eenheid
Stap (kHz)
Demodulatie
Spectrum
Ontvangen / toepassen
Scan
X-breedte
X-verschuiving (kHz)
Y min (dBFS)
Y max (dBFS)
Assen toepassen
Frequentie onthouden
Frequentie vasthouden
Voorgesteld bereik
Tuner
Direct I
Direct Q
De bereiken zijn aanpasbare suggesties. De dongle bepaalt welke frequenties worden geaccepteerd. Direct I/Q vereist een geschikte hardware-ingang.
Begin (kHz)
Einde (kHz)
Duur per stap (ms)
Drempel (dBFS)
Scannen pauzeren bij een signaal
Bereik scannen
Eén doorgang, maximaal 20.000 stappen. De drempel detecteert een signaal zonder de zender te identificeren. Stel de stap hierboven in.
Selectie ontvangen
Lijst leegmaken
Lijst opslaan
Maximaal 128 detecties. Alleen selecteren verandert de ontvangst niet. Opslaan bewaart ook verwijderingen.
Rapporten ontvangen / geweigerd
USB-fouten / herstarts
Media filteren
Selectie opgeslagen
Onvoldoende geheugen voor de medialijst''',
'es-ES': '''Ordenar por
Nombre: de A a Z
Nombre: de Z a A
Fecha: más antiguas primero
Fecha: más recientes primero
Tamaño: ascendente
Tamaño: descendente
Última vista no guardada
Seleccione Abrir para mostrar esta imagen.
Cargando.
Memoria insuficiente - utilice la búsqueda
Ajustes del mapa
Recordar la última vista
La posición y el zoom se guardan al salir de Mapas. Desactivado: abrir la vista inicial.
Detecciones pendientes: barrido en pausa
Frecuencia o entrada rechazada por el receptor USB
Tasa de muestreo rechazada por el receptor USB
Barrido en curso
Barrido terminado
Espectro activo: salida de audio ocupada
Señal detectada: escucha con el barrido en pausa
Frecuencia o modo no válido
Buscando el sintonizador
Deteniendo
Barrido no válido o demasiado amplio
Barrido en pausa: frecuencia fija
Personalizada
Onda larga
Onda media
Onda corta
FM 64–108 MHz
FM 87,5–108 MHz
VHF aeronáutica
VHF 144–148 MHz
VHF marítima
UHF 430–440 MHz
UHF 446 MHz
UHF 863–870 MHz
Frecuencia no válida
Sin detecciones
Detecciones
Archivo de detecciones no válido
Detecciones guardadas en /System/SDR/detections.csv
Paso de frecuencia no válido
Elija AM, WFM o NFM para escuchar
Ejes no válidos: el mínimo debe ser menor que el máximo
Lista llena: elimine detecciones
Frecuencia
Unidad
Paso (kHz)
Demodulación
Espectro
Recibir / aplicar
Barrido
Anchura X
Desplazamiento X (kHz)
Y mín. (dBFS)
Y máx. (dBFS)
Aplicar ejes
Memorizar
Fijar frecuencia
Intervalo sugerido
Sintonizador
Directa I
Directa Q
Los intervalos son orientativos y modificables. El receptor determina las frecuencias aceptadas. Directa I/Q requiere una entrada de hardware adecuada.
Inicio (kHz)
Fin (kHz)
Duración por paso (ms)
Umbral (dBFS)
Pausar el barrido al detectar una señal
Barrido del intervalo
Una pasada, máximo 20.000 pasos. El umbral detecta una señal sin identificar la emisora. El paso se ajusta arriba.
Recibir la selección
Vaciar lista
Guardar lista
Máximo 128 detecciones. Seleccionar no cambia la recepción. Guardar también conserva las eliminaciones.
Informes recibidos / rechazados
Errores USB / reinicios
Filtrar medios
Selección guardada
Memoria insuficiente para la lista de medios''',
'it-IT': '''Ordina per
Nome: dalla A alla Z
Nome: dalla Z alla A
Data: meno recenti prima
Data: più recenti prima
Dimensione: crescente
Dimensione: decrescente
Ultima vista non salvata
Selezionare Apri per visualizzare questa immagine.
Caricamento.
Memoria insufficiente - usare la ricerca
Impostazioni della mappa
Ricorda l'ultima vista
Posizione e zoom vengono salvati uscendo da Mappe. Se disattivato: apre la vista iniziale.
Rilevamenti in attesa: scansione in pausa
Frequenza o ingresso rifiutato dal ricevitore USB
Frequenza di campionamento rifiutata dal ricevitore USB
Scansione in corso
Scansione completata
Spettro attivo: uscita audio occupata
Segnale rilevato: ascolto con scansione in pausa
Frequenza o modalità non valida
Ricerca del sintonizzatore
Arresto in corso
Scansione non valida o troppo estesa
Scansione in pausa: frequenza fissa
Personalizzata
Onde lunghe
Onde medie
Onde corte
FM 64–108 MHz
FM 87,5–108 MHz
VHF aeronautica
VHF 144–148 MHz
VHF marittima
UHF 430–440 MHz
UHF 446 MHz
UHF 863–870 MHz
Frequenza non valida
Nessun rilevamento
Rilevamenti
File dei rilevamenti non valido
Rilevamenti salvati in /System/SDR/detections.csv
Passo di frequenza non valido
Scegliere AM, WFM o NFM per ascoltare
Assi non validi: il minimo deve essere inferiore al massimo
Elenco pieno: eliminare alcuni rilevamenti
Frequenza
Unità
Passo (kHz)
Demodulazione
Spettro
Ricevi / applica
Scansione
Larghezza X
Scostamento X (kHz)
Y min (dBFS)
Y max (dBFS)
Applica assi
Memorizza
Mantieni frequenza
Intervallo suggerito
Sintonizzatore
Diretto I
Diretto Q
Gli intervalli sono indicativi e modificabili. Il ricevitore determina le frequenze accettate. Diretto I/Q richiede un ingresso hardware adatto.
Inizio (kHz)
Fine (kHz)
Durata per passo (ms)
Soglia (dBFS)
Sospendi la scansione su un segnale
Scansiona l'intervallo
Un passaggio, massimo 20.000 passi. La soglia rileva un segnale senza identificare la stazione. Il passo si imposta sopra.
Ricevi la selezione
Svuota elenco
Salva elenco
Massimo 128 rilevamenti. La sola selezione non modifica la ricezione. Il salvataggio conserva anche le eliminazioni.
Rapporti ricevuti / rifiutati
Errori USB / riavvii
Filtra contenuti multimediali
Selezione salvata
Memoria insufficiente per l'elenco multimediale''',
'pt-BR': '''Ordenar por
Nome: de A a Z
Nome: de Z a A
Data: mais antigas primeiro
Data: mais recentes primeiro
Tamanho: crescente
Tamanho: decrescente
Última visualização não salva
Selecione Abrir para exibir esta imagem.
Carregando.
Memória insuficiente - use a pesquisa
Configurações do mapa
Lembrar a última visualização
A posição e o zoom são salvos ao sair de Mapas. Desativado: abrir a visualização inicial.
Detecções pendentes: varredura pausada
Frequência ou entrada recusada pelo receptor USB
Taxa de amostragem recusada pelo receptor USB
Varredura em andamento
Varredura concluída
Espectro ativo: saída de áudio ocupada
Sinal detectado: escuta com a varredura pausada
Frequência ou modo inválido
Procurando o sintonizador
Parando
Varredura inválida ou muito extensa
Varredura pausada: frequência fixa
Personalizada
Ondas longas
Ondas médias
Ondas curtas
FM 64–108 MHz
FM 87,5–108 MHz
VHF aeronáutica
VHF 144–148 MHz
VHF marítima
UHF 430–440 MHz
UHF 446 MHz
UHF 863–870 MHz
Frequência inválida
Nenhuma detecção
Detecções
Arquivo de detecções inválido
Detecções salvas em /System/SDR/detections.csv
Passo de frequência inválido
Escolha AM, WFM ou NFM para ouvir
Eixos inválidos: o mínimo deve ser menor que o máximo
Lista cheia: exclua detecções
Frequência
Unidade
Passo (kHz)
Demodulação
Espectro
Receber / aplicar
Varredura
Largura X
Deslocamento X (kHz)
Y mín. (dBFS)
Y máx. (dBFS)
Aplicar eixos
Memorizar
Fixar frequência
Faixa sugerida
Sintonizador
Direta I
Direta Q
As faixas são indicativas e podem ser alteradas. O receptor determina as frequências aceitas. Direta I/Q exige uma entrada de hardware adequada.
Início (kHz)
Fim (kHz)
Duração por passo (ms)
Limiar (dBFS)
Pausar a varredura ao detectar um sinal
Varrer a faixa
Uma passagem, no máximo 20.000 passos. O limiar detecta um sinal sem identificar a estação. O passo é ajustado acima.
Receber seleção
Limpar lista
Salvar lista
No máximo 128 detecções. Apenas selecionar não altera a recepção. Salvar também preserva as exclusões.
Relatórios recebidos / rejeitados
Erros USB / reinícios
Filtrar mídias
Seleção salva
Memória insuficiente para a lista de mídias''',
'pt-PT': '''Ordenar por
Nome: de A a Z
Nome: de Z a A
Data: mais antigas primeiro
Data: mais recentes primeiro
Tamanho: crescente
Tamanho: decrescente
Última vista não guardada
Selecione Abrir para apresentar esta imagem.
A carregar.
Memória insuficiente - utilize a pesquisa
Definições do mapa
Memorizar a última vista
A posição e o zoom são guardados ao sair de Mapas. Desativado: abrir a vista inicial.
Deteções pendentes: varrimento em pausa
Frequência ou entrada recusada pelo recetor USB
Taxa de amostragem recusada pelo recetor USB
Varrimento em curso
Varrimento concluído
Espetro ativo: saída de áudio ocupada
Sinal detetado: escuta com o varrimento em pausa
Frequência ou modo inválido
A procurar o sintonizador
A parar
Varrimento inválido ou demasiado extenso
Varrimento em pausa: frequência fixa
Personalizada
Ondas longas
Ondas médias
Ondas curtas
FM 64–108 MHz
FM 87,5–108 MHz
VHF aeronáutica
VHF 144–148 MHz
VHF marítima
UHF 430–440 MHz
UHF 446 MHz
UHF 863–870 MHz
Frequência inválida
Nenhuma deteção
Deteções
Ficheiro de deteções inválido
Deteções guardadas em /System/SDR/detections.csv
Passo de frequência inválido
Escolha AM, WFM ou NFM para ouvir
Eixos inválidos: o mínimo deve ser inferior ao máximo
Lista cheia: elimine deteções
Frequência
Unidade
Passo (kHz)
Desmodulação
Espetro
Receber / aplicar
Varrimento
Largura X
Deslocamento X (kHz)
Y mín. (dBFS)
Y máx. (dBFS)
Aplicar eixos
Memorizar
Fixar frequência
Faixa sugerida
Sintonizador
Direta I
Direta Q
As faixas são indicativas e podem ser alteradas. O recetor determina as frequências aceites. Direta I/Q exige uma entrada de hardware adequada.
Início (kHz)
Fim (kHz)
Duração por passo (ms)
Limiar (dBFS)
Suspender o varrimento ao detetar um sinal
Varrer a faixa
Uma passagem, no máximo 20 000 passos. O limiar deteta um sinal sem identificar a estação. O passo é definido acima.
Receber seleção
Limpar lista
Guardar lista
No máximo 128 deteções. A seleção por si só não altera a receção. Guardar também preserva as eliminações.
Relatórios recebidos / rejeitados
Erros USB / reinícios
Filtrar conteúdos multimédia
Seleção guardada
Memória insuficiente para a lista multimédia''',
}


def main():
    delta = {'fr-FR': {key: key for key in KEYS}}
    for locale, block in ROWS.items():
        values = block.splitlines()
        assert len(values) == len(KEYS), (locale, len(values), len(KEYS))
        delta[locale] = dict(zip(KEYS, values))
    (HERE/'delta-west-0824.json').write_text(json.dumps(delta, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    for locale, additions in delta.items():
        target = SKETCH/'sdcard/System/languages'/(locale+'.json')
        pack = json.loads(target.read_text(encoding='utf-8'))
        pack['translations'].update(additions)
        target.write_text(json.dumps(pack, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('Applied', len(KEYS), 'keys in', len(delta), 'language packs')


if __name__ == '__main__':
    main()
