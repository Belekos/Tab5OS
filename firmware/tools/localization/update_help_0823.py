"""Update the embedded help and all fourteen packs from reviewed translations."""
from pathlib import Path
import json
import re

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SKETCH = next(ROOT.glob('Tab5OS_Arduino_*_Ecriture'))
TITLE = 'Menus et consoles'
BODY = "Dans l’explorateur, le menu regroupe les actions secondaires. Ses commandes dépendent de la sélection ; Git concerne un dossier sélectionné. Le clic droit ouvre les actions disponibles à cet endroit.\n\nDans une console, Ctrl+L ou Effacer la sortie vide le texte affiché. Pendant un travail, les nouvelles lignes continuent de s’afficher ; effacer ne signifie pas arrêter. Le bouton Accueil est masqué lorsque vous êtes déjà dans le menu principal."
TRANSLATIONS = {
    'fr-FR': BODY,
    'en-US': "In the file explorer, the menu groups secondary actions. Its commands depend on the selection; Git applies to a selected folder. Right-click opens the actions available at that location.\n\nIn a console, Ctrl+L or Clear output empties the displayed text. During a job, new lines continue to appear; clearing does not stop the job. The Home button is hidden while you are already in the main menu.",
    'de-DE': "Im Dateimanager fasst das Menü zusätzliche Aktionen zusammen. Seine Befehle hängen von der Auswahl ab; Git gilt für einen ausgewählten Ordner. Ein Rechtsklick öffnet die an dieser Stelle verfügbaren Aktionen.\n\nIn einer Konsole leert Ctrl+L oder Ausgabe leeren den angezeigten Text. Während eines Vorgangs erscheinen weiterhin neue Zeilen; das Leeren beendet den Vorgang nicht. Die Startschaltfläche ist im Hauptmenü ausgeblendet.",
    'es-ES': "En el explorador, el menú agrupa las acciones secundarias. Sus comandos dependen de la selección; Git se aplica a una carpeta seleccionada. El clic derecho abre las acciones disponibles en ese lugar.\n\nEn una consola, Ctrl+L o Borrar la salida vacía el texto mostrado. Durante una tarea siguen apareciendo líneas nuevas; borrar no detiene la tarea. El botón Inicio se oculta cuando ya estás en el menú principal.",
    'it-IT': "Nel gestore file, il menu raggruppa le azioni secondarie. I comandi dipendono dalla selezione; Git riguarda una cartella selezionata. Il clic destro apre le azioni disponibili in quel punto.\n\nIn una console, Ctrl+L o Cancella l'output svuota il testo visualizzato. Durante un'attività continuano ad apparire nuove righe; cancellare non interrompe l'attività. Il pulsante Home è nascosto quando ci si trova già nel menu principale.",
    'nl-NL': "In de bestandsverkenner groepeert het menu de aanvullende acties. De opdrachten hangen af van de selectie; Git geldt voor een geselecteerde map. Rechtsklikken opent de acties die op die plaats beschikbaar zijn.\n\nIn een console wist Ctrl+L of Uitvoer wissen de weergegeven tekst. Tijdens een taak blijven nieuwe regels verschijnen; wissen stopt de taak niet. De startknop is verborgen wanneer het hoofdmenu al geopend is.",
    'pt-BR': "No explorador de arquivos, o menu agrupa as ações secundárias. Os comandos dependem da seleção; Git se aplica a uma pasta selecionada. O clique direito abre as ações disponíveis naquele local.\n\nEm um console, Ctrl+L ou Limpar a saída apaga o texto exibido. Durante uma tarefa, novas linhas continuam aparecendo; limpar não interrompe a tarefa. O botão Início fica oculto quando você já está no menu principal.",
    'pt-PT': "No explorador de ficheiros, o menu agrupa as ações secundárias. Os comandos dependem da seleção; Git aplica-se a uma pasta selecionada. O clique direito abre as ações disponíveis nesse local.\n\nNuma consola, Ctrl+L ou Limpar a saída apaga o texto apresentado. Durante uma tarefa, continuam a aparecer novas linhas; limpar não interrompe a tarefa. O botão Início fica oculto quando já está no menu principal.",
    'id-ID': "Di penjelajah berkas, menu mengelompokkan tindakan tambahan. Perintahnya bergantung pada pilihan; Git berlaku untuk folder yang dipilih. Klik kanan membuka tindakan yang tersedia di lokasi tersebut.\n\nDi konsol, Ctrl+L atau Hapus keluaran mengosongkan teks yang ditampilkan. Selama tugas berjalan, baris baru tetap muncul; menghapus tidak menghentikan tugas. Tombol Beranda disembunyikan saat Anda sudah berada di menu utama.",
    'ru-RU': "В проводнике меню объединяет дополнительные действия. Команды зависят от выделения; Git применяется к выбранной папке. Щелчок правой кнопкой открывает действия, доступные в этом месте.\n\nВ консоли Ctrl+L или «Очистить вывод» удаляет отображаемый текст. Во время работы новые строки продолжают появляться; очистка не останавливает задачу. Кнопка главного экрана скрыта, если главное меню уже открыто.",
    'ar': "في مستكشف الملفات، تجمع القائمة الإجراءات الإضافية. تعتمد أوامرها على التحديد؛ ويخص Git المجلد المحدد. يفتح النقر بالزر الأيمن الإجراءات المتاحة في ذلك الموضع.\n\nفي نافذة الأوامر، يمسح Ctrl+L أو مسح المخرجات النص المعروض. أثناء العمل تستمر الأسطر الجديدة في الظهور؛ المسح لا يوقف المهمة. يختفي زر الرئيسية عندما تكون القائمة الرئيسية مفتوحة بالفعل.",
    'zh-CN': "在文件管理器中，菜单集中显示次要操作。可用命令取决于所选项目；Git 适用于选中的文件夹。单击鼠标右键可打开当前位置的可用操作。\n\n在控制台中，Ctrl+L 或“清除输出”会清空显示的文本。任务进行时，新行仍会继续显示；清除不会停止任务。已经位于主菜单时，主页按钮会隐藏。",
    'ja-JP': "ファイル管理では、追加の操作をメニューにまとめています。コマンドは選択項目によって変わり、Git は選択したフォルダーが対象です。右クリックすると、その場所で利用できる操作が開きます。\n\nコンソールでは、Ctrl+L または「出力を消去」で表示中のテキストを消します。処理中は新しい行が引き続き表示され、消去しても処理は停止しません。メインメニューを開いている間はホームボタンが非表示になります。",
    'ko-KR': "파일 탐색기의 메뉴에는 추가 작업이 모여 있습니다. 명령은 선택 항목에 따라 달라지며 Git은 선택한 폴더에 적용됩니다. 마우스 오른쪽 버튼을 누르면 해당 위치에서 사용할 수 있는 작업이 열립니다.\n\n콘솔에서 Ctrl+L 또는 출력 지우기를 사용하면 표시된 텍스트가 지워집니다. 작업 중에는 새 줄이 계속 표시되며 지워도 작업이 중지되지는 않습니다. 이미 주 메뉴에 있으면 홈 버튼이 숨겨집니다.",
}


def apply():
    source_path = SKETCH / 'Tab5Help.cpp'
    source = source_path.read_text(encoding='utf8')
    match = re.search(r'\{"Terminal",\s*("(?:\\.|[^"\\])*")\}', source)
    assert match, 'Terminal help page not found'
    french = json.loads((SKETCH/'sdcard/System/languages/fr-FR.json').read_text(encoding='utf8'))['translations']
    old_key = next(k for k in french if k.startswith('Le terminal fonctionne aussi') and 'Pas de tubes' in k)
    fragments = [next(k for k in french if k.startswith(prefix)) for prefix in ('Tubes : commande | filtre.', 'Limites : 16 commandes par tube,', 'Les guillemets doubles conservent')]
    new_key = '\n\n'.join(old_key.split('\n\n')[:3] + fragments + [old_key.split('\n\n')[-1]])
    assert 'Pas de tubes' not in new_key
    delta_path = HERE / 'delta-ui-0823.json'
    delta = json.loads(delta_path.read_text(encoding='utf8'))
    for code, body in TRANSLATIONS.items():
        path = SKETCH/'sdcard/System/languages'/f'{code}.json'
        pack = json.loads(path.read_text(encoding='utf8'))
        rows = pack['translations']
        assert old_key in rows and all(k in rows for k in fragments)
        paragraphs = rows[old_key].split('\n\n')
        assert len(paragraphs) == 5, (code, len(paragraphs))
        revised = '\n\n'.join(paragraphs[:3] + [rows[k] for k in fragments] + [paragraphs[-1]])
        rows[new_key] = revised
        rows[BODY] = body
        delta[code].update({new_key: revised, BODY: body})
        path.write_text(json.dumps(pack, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    source = source[:match.start(1)] + json.dumps(new_key, ensure_ascii=False) + source[match.end(1):]
    assert source.count('\n};\n') == 1
    if '{'+json.dumps(TITLE, ensure_ascii=False)+',' not in source:
        source = source.replace('\n};\n', '\n  {'+json.dumps(TITLE, ensure_ascii=False)+', '+json.dumps(BODY, ensure_ascii=False)+'},\n};\n')
    source_path.write_text(source, encoding='utf8')
    delta_path.write_text(json.dumps(delta, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    (HERE/'reference-delta-0823.json').write_text(json.dumps(list(delta['fr-FR']), ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    print('Updated Terminal help and added Menus/consoles help in 14 packs.')


if __name__ == '__main__':
    apply()
