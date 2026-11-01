from pathlib import Path
import json
HERE=Path(__file__).resolve().parent
ROWS={
'fr-FR':[
"Dans Explorateur, Haut/Bas sélectionnent, Entrée ouvre et Retour arrière remonte au dossier parent. Un clic sélectionne ; un double-clic ouvre. Au toucher, le premier appui sélectionne et un nouvel appui sur la sélection ouvre, sans délai imposé. Sélectionner une image ne l'ouvre pas. Copier / Couper prépare l'élément ; Coller agit dans le dossier courant.",
"Ouvrir ou Exécuter un script .sh lance le terminal dans le dossier du script. Pour le modifier, utilisez Ouvrir avec.",
"En WFM, l'écoute mono filtre les composantes stéréo et RDS. La désaccentuation vaut généralement 50 µs en Europe ou 75 µs selon le pays. Correction (ppm) compense l'écart de l'oscillateur de la clé ; commencez à 0 et ajustez avec une fréquence connue. Une antenne adaptée et un niveau radio suffisant restent nécessaires."],
'en-US':[
"In Explorer, Up/Down selects, Enter opens and Backspace goes to the parent folder. A click selects; a double-click opens. With touch, the first tap selects and another tap on the selected item opens it, with no time limit. Selecting an image does not open it. Copy / Cut prepares the item; Paste acts in the current folder.",
'Open or Run on a .sh script starts the terminal in the script’s folder. To edit it, use Open with.',
'In WFM, mono audio filters out stereo and RDS components. De-emphasis is generally 50 µs in Europe or 75 µs depending on the country. Correction (ppm) compensates for the dongle’s oscillator error; start at 0 and adjust using a known frequency. A suitable antenna and sufficient radio signal are still needed.'],
'de-DE':[
'Im Explorer wählen Auf/Ab aus, Eingabe öffnet und die Rücktaste wechselt zum übergeordneten Ordner. Ein Klick wählt aus; ein Doppelklick öffnet. Bei Berührung wählt das erste Tippen aus; erneutes Tippen auf die Auswahl öffnet sie, ohne Zeitbegrenzung. Das Auswählen eines Bildes öffnet es nicht. Kopieren / Ausschneiden bereitet das Element vor; Einfügen wirkt im aktuellen Ordner.',
'Öffnen oder Ausführen eines .sh-Skripts startet das Terminal im Ordner des Skripts. Zum Bearbeiten verwenden Sie Öffnen mit.',
'Bei WFM filtert die Monoausgabe Stereo- und RDS-Komponenten. Die Deemphasis beträgt üblicherweise 50 µs in Europa oder je nach Land 75 µs. Korrektur (ppm) gleicht die Oszillatorabweichung des Sticks aus; beginnen Sie bei 0 und passen Sie den Wert anhand einer bekannten Frequenz an. Eine passende Antenne und ausreichende Signalstärke bleiben erforderlich.'],
'es-ES':[
'En Explorador, Arriba/Abajo selecciona, Intro abre y Retroceso sube a la carpeta superior. Un clic selecciona; un doble clic abre. En la pantalla táctil, el primer toque selecciona y otro toque sobre la selección abre, sin límite de tiempo. Seleccionar una imagen no la abre. Copiar / Cortar prepara el elemento; Pegar actúa en la carpeta actual.',
'Abrir o Ejecutar un script .sh inicia el terminal en la carpeta del script. Para editarlo, usa Abrir con.',
'En WFM, el audio mono filtra los componentes estéreo y RDS. El desénfasis suele ser de 50 µs en Europa o 75 µs según el país. Corrección (ppm) compensa la desviación del oscilador del receptor; empieza en 0 y ajusta con una frecuencia conocida. Sigue siendo necesaria una antena adecuada y suficiente señal de radio.'],
'it-IT':[
'In Esplora file, Su/Giù seleziona, Invio apre e Backspace risale alla cartella superiore. Un clic seleziona; un doppio clic apre. Sul touchscreen, il primo tocco seleziona e un altro tocco sulla selezione apre, senza limite di tempo. Selezionare un’immagine non la apre. Copia / Taglia prepara l’elemento; Incolla agisce nella cartella corrente.',
'Aprire o Eseguire uno script .sh avvia il terminale nella cartella dello script. Per modificarlo, usa Apri con.',
'In WFM, l’audio mono filtra le componenti stereo e RDS. La deenfasi è generalmente di 50 µs in Europa o 75 µs a seconda del paese. Correzione (ppm) compensa lo scarto dell’oscillatore della chiavetta; inizia da 0 e regola usando una frequenza nota. Servono comunque un’antenna adatta e un segnale radio sufficiente.'],
'nl-NL':[
'In Verkenner selecteert Omhoog/Omlaag, opent Enter en gaat Backspace naar de bovenliggende map. Eén klik selecteert; een dubbelklik opent. Bij aanraking selecteert de eerste tik en opent een volgende tik op de selectie, zonder tijdslimiet. Een afbeelding selecteren opent deze niet. Kopiëren / Knippen bereidt het item voor; Plakken werkt in de huidige map.',
'Openen of Uitvoeren van een .sh-script start de terminal in de map van het script. Gebruik Openen met om het te bewerken.',
'Bij WFM filtert mono-audio de stereo- en RDS-componenten weg. De deëmfase is meestal 50 µs in Europa of 75 µs, afhankelijk van het land. Correctie (ppm) compenseert de oscillatorafwijking van de dongle; begin bij 0 en stel bij met een bekende frequentie. Een geschikte antenne en voldoende radiosignaal blijven nodig.'],
'pt-BR':[
'No Explorador, Cima/Baixo seleciona, Enter abre e Backspace volta à pasta superior. Um clique seleciona; um clique duplo abre. Na tela sensível ao toque, o primeiro toque seleciona e outro toque na seleção abre, sem limite de tempo. Selecionar uma imagem não a abre. Copiar / Recortar prepara o item; Colar atua na pasta atual.',
'Abrir ou Executar um script .sh inicia o terminal na pasta do script. Para editá-lo, use Abrir com.',
'Em WFM, o áudio mono filtra os componentes estéreo e RDS. A deênfase costuma ser de 50 µs na Europa ou 75 µs conforme o país. Correção (ppm) compensa o desvio do oscilador do receptor; comece em 0 e ajuste usando uma frequência conhecida. Uma antena adequada e um sinal de rádio suficiente continuam necessários.'],
'pt-PT':[
'No Explorador, Cima/Baixo seleciona, Enter abre e Backspace volta à pasta superior. Um clique seleciona; um duplo clique abre. No ecrã tátil, o primeiro toque seleciona e outro toque na seleção abre, sem limite de tempo. Selecionar uma imagem não a abre. Copiar / Cortar prepara o elemento; Colar atua na pasta atual.',
'Abrir ou Executar um script .sh inicia o terminal na pasta do script. Para o editar, use Abrir com.',
'Em WFM, o áudio mono filtra os componentes estéreo e RDS. A desênfase é geralmente de 50 µs na Europa ou 75 µs consoante o país. Correção (ppm) compensa o desvio do oscilador do recetor; comece em 0 e ajuste com uma frequência conhecida. Continuam a ser necessários uma antena adequada e um sinal de rádio suficiente.']}
delta={locale:dict(zip(('files_first','files_script','sdr_quality'),values)) for locale,values in ROWS.items()}
(HERE/'help-west-0825.json').write_text(json.dumps(delta,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('Prepared 3 help segments in',len(delta),'western/source languages')
