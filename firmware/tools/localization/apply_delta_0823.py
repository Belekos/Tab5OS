"""Apply reviewed .23 UI translations; fail rather than pad incomplete lists."""
from pathlib import Path
import json

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SKETCH=next(ROOT.glob('Tab5OS_Arduino_*_Ecriture'))
KEYS='''Analyse en cours.
Effacer la sortie
Effacer la sortie · Ctrl+L
Informations
Masquer ou afficher la sélection
Initialiser ce dossier avec Git
Sélection multiple
Ne plus afficher les fichiers cachés
Quitter la sélection multiple
Afficher les fichiers cachés
Aucune commande reçue
Commandes non reconnues
Manette
Réglage non enregistré
Téléchargement indisponible : ressources occupées
Cette source autorise la consultation uniquement. Choisir une autre source pour importer.
Zoom minimum
Le serveur limite les téléchargements. Réessayer plus tard.
Zone trop grande : réduire les détails ou rapprocher la carte.
Zoom maximum
En pause
Télécharger la zone
Limite de la source
Téléchargement en cours
Téléchargement terminé
Télécharger / reprendre
Zone affichée
Téléchargement interrompu. Les tuiles enregistrées sont conservées.
Enregistrement impossible. Les tuiles enregistrées sont conservées.
Définir l’accueil ici
Vue d'ensemble\\nDNS\\nRéponse réseau\\nPorts TCP\\nHTTP et TLS
Adresse IP ou nom d'hôte
Entrée
Suppr
Menus et consoles'''.splitlines()
KEYS=[s.replace('\\n','\n') for s in KEYS]
ROWS={
'en-US':'''Analysis in progress.
Clear output
Clear output · Ctrl+L
Information
Hide or show selection
Initialize this folder with Git
Multiple selection
Hide hidden files
Exit multiple selection
Show hidden files
No input received
Unrecognized input
Controller
Setting not saved
Download unavailable: resources busy
This source allows viewing only. Choose another source to import.
Minimum zoom
The server is limiting downloads. Try again later.
Area too large: reduce detail or zoom in on the map.
Maximum zoom
Paused
Download area
Source limit
Download in progress
Download complete
Download / resume
Displayed area
Download interrupted. Saved tiles are kept.
Unable to save. Saved tiles are kept.
Set home here
Overview\\nDNS\\nNetwork response\\nTCP ports\\nHTTP and TLS
IP address or hostname
Enter
Del
Menus and consoles''',
'de-DE':'''Analyse läuft.
Ausgabe leeren
Ausgabe leeren · Ctrl+L
Informationen
Auswahl aus- oder einblenden
Diesen Ordner mit Git initialisieren
Mehrfachauswahl
Versteckte Dateien ausblenden
Mehrfachauswahl beenden
Versteckte Dateien anzeigen
Keine Eingabe empfangen
Eingaben nicht erkannt
Controller
Einstellung nicht gespeichert
Download nicht verfügbar: Ressourcen belegt
Diese Quelle erlaubt nur die Ansicht. Zum Importieren eine andere Quelle wählen.
Minimaler Zoom
Der Server begrenzt Downloads. Später erneut versuchen.
Bereich zu groß: Details verringern oder in die Karte hineinzoomen.
Maximaler Zoom
Pausiert
Bereich herunterladen
Quellenlimit
Download läuft
Download abgeschlossen
Herunterladen / fortsetzen
Angezeigter Bereich
Download unterbrochen. Gespeicherte Kacheln bleiben erhalten.
Speichern nicht möglich. Gespeicherte Kacheln bleiben erhalten.
Hier als Startposition festlegen
Übersicht\\nDNS\\nNetzwerkantwort\\nTCP-Ports\\nHTTP und TLS
IP-Adresse oder Hostname
Eingabe
Entf
Menüs und Konsolen''',
'es-ES':'''Análisis en curso.
Borrar la salida
Borrar la salida · Ctrl+L
Información
Ocultar o mostrar la selección
Inicializar esta carpeta con Git
Selección múltiple
Ocultar los archivos ocultos
Salir de la selección múltiple
Mostrar los archivos ocultos
No se ha recibido ninguna entrada
Entradas no reconocidas
Mando
Ajuste no guardado
Descarga no disponible: recursos ocupados
Esta fuente solo permite la consulta. Elige otra fuente para importar.
Zoom mínimo
El servidor limita las descargas. Inténtalo más tarde.
Zona demasiado grande: reduce el detalle o acerca el mapa.
Zoom máximo
En pausa
Descargar la zona
Límite de la fuente
Descarga en curso
Descarga terminada
Descargar / reanudar
Zona mostrada
Descarga interrumpida. Se conservan las teselas guardadas.
No se puede guardar. Se conservan las teselas guardadas.
Fijar el inicio aquí
Resumen\\nDNS\\nRespuesta de red\\nPuertos TCP\\nHTTP y TLS
Dirección IP o nombre del servidor
Intro
Supr
Menús y consolas''',
'it-IT':'''Analisi in corso.
Cancella l'output
Cancella l'output · Ctrl+L
Informazioni
Nascondi o mostra la selezione
Inizializza questa cartella con Git
Selezione multipla
Nascondi i file nascosti
Esci dalla selezione multipla
Mostra i file nascosti
Nessun input ricevuto
Input non riconosciuti
Controller
Impostazione non salvata
Download non disponibile: risorse occupate
Questa fonte consente solo la consultazione. Scegli un'altra fonte per importare.
Zoom minimo
Il server limita i download. Riprova più tardi.
Area troppo grande: riduci i dettagli o avvicina la mappa.
Zoom massimo
In pausa
Scarica l'area
Limite della fonte
Download in corso
Download completato
Scarica / riprendi
Area visualizzata
Download interrotto. Le tessere salvate vengono conservate.
Impossibile salvare. Le tessere salvate vengono conservate.
Imposta qui la posizione iniziale
Panoramica\\nDNS\\nRisposta di rete\\nPorte TCP\\nHTTP e TLS
Indirizzo IP o nome host
Invio
Canc
Menu e console''',
'nl-NL':'''Analyse wordt uitgevoerd.
Uitvoer wissen
Uitvoer wissen · Ctrl+L
Informatie
Selectie verbergen of tonen
Deze map met Git initialiseren
Meervoudige selectie
Verborgen bestanden verbergen
Meervoudige selectie afsluiten
Verborgen bestanden tonen
Geen invoer ontvangen
Invoer niet herkend
Controller
Instelling niet opgeslagen
Download niet beschikbaar: bronnen bezet
Deze bron staat alleen bekijken toe. Kies een andere bron om te importeren.
Minimale zoom
De server beperkt downloads. Probeer het later opnieuw.
Gebied te groot: verminder details of zoom in op de kaart.
Maximale zoom
Gepauzeerd
Gebied downloaden
Bronlimiet
Download wordt uitgevoerd
Download voltooid
Downloaden / hervatten
Weergegeven gebied
Download onderbroken. Opgeslagen tegels blijven behouden.
Opslaan niet mogelijk. Opgeslagen tegels blijven behouden.
Startpositie hier instellen
Overzicht\\nDNS\\nNetwerkrespons\\nTCP-poorten\\nHTTP en TLS
IP-adres of hostnaam
Enter
Del
Menu's en consoles''',
'pt-BR':'''Análise em andamento.
Limpar a saída
Limpar a saída · Ctrl+L
Informações
Ocultar ou mostrar a seleção
Inicializar esta pasta com Git
Seleção múltipla
Ocultar os arquivos ocultos
Sair da seleção múltipla
Mostrar os arquivos ocultos
Nenhum comando recebido
Comandos não reconhecidos
Controle
Configuração não salva
Download indisponível: recursos ocupados
Esta fonte permite apenas a consulta. Escolha outra fonte para importar.
Zoom mínimo
O servidor está limitando os downloads. Tente mais tarde.
Área muito grande: reduza os detalhes ou aproxime o mapa.
Zoom máximo
Pausado
Baixar a área
Limite da fonte
Download em andamento
Download concluído
Baixar / retomar
Área exibida
Download interrompido. Os blocos salvos são mantidos.
Não foi possível salvar. Os blocos salvos são mantidos.
Definir o início aqui
Visão geral\\nDNS\\nResposta da rede\\nPortas TCP\\nHTTP e TLS
Endereço IP ou nome do servidor
Enter
Del
Menus e consoles''',
'pt-PT':'''Análise em curso.
Limpar a saída
Limpar a saída · Ctrl+L
Informações
Ocultar ou mostrar a seleção
Inicializar esta pasta com Git
Seleção múltipla
Ocultar os ficheiros ocultos
Sair da seleção múltipla
Mostrar os ficheiros ocultos
Nenhum comando recebido
Comandos não reconhecidos
Comando
Definição não guardada
Transferência indisponível: recursos ocupados
Esta fonte permite apenas a consulta. Escolha outra fonte para importar.
Zoom mínimo
O servidor está a limitar as transferências. Tente mais tarde.
Área demasiado grande: reduza os detalhes ou aproxime o mapa.
Zoom máximo
Em pausa
Transferir a área
Limite da fonte
Transferência em curso
Transferência concluída
Transferir / retomar
Área apresentada
Transferência interrompida. Os mosaicos guardados são mantidos.
Não foi possível guardar. Os mosaicos guardados são mantidos.
Definir o início aqui
Vista geral\\nDNS\\nResposta da rede\\nPortas TCP\\nHTTP e TLS
Endereço IP ou nome do servidor
Enter
Del
Menus e consolas''',
'id-ID':'''Analisis berlangsung.
Hapus keluaran
Hapus keluaran · Ctrl+L
Informasi
Sembunyikan atau tampilkan pilihan
Inisialisasi folder ini dengan Git
Pilihan jamak
Sembunyikan berkas tersembunyi
Keluar dari pilihan jamak
Tampilkan berkas tersembunyi
Belum ada masukan diterima
Masukan tidak dikenali
Pengendali
Pengaturan tidak tersimpan
Unduhan tidak tersedia: sumber daya sibuk
Sumber ini hanya mengizinkan penayangan. Pilih sumber lain untuk mengimpor.
Zoom minimum
Server membatasi unduhan. Coba lagi nanti.
Area terlalu besar: kurangi detail atau perbesar tampilan peta.
Zoom maksimum
Dijeda
Unduh area
Batas sumber
Unduhan berlangsung
Unduhan selesai
Unduh / lanjutkan
Area yang ditampilkan
Unduhan terputus. Ubin yang tersimpan tetap dipertahankan.
Tidak dapat menyimpan. Ubin yang tersimpan tetap dipertahankan.
Tetapkan posisi awal di sini
Ringkasan\\nDNS\\nRespons jaringan\\nPort TCP\\nHTTP dan TLS
Alamat IP atau nama host
Enter
Del
Menu dan konsol''',
'ru-RU':'''Выполняется анализ.
Очистить вывод
Очистить вывод · Ctrl+L
Сведения
Скрыть или показать выделенное
Инициализировать Git в этой папке
Множественный выбор
Не показывать скрытые файлы
Выйти из множественного выбора
Показать скрытые файлы
Ввод не получен
Команды не распознаны
Геймпад
Настройка не сохранена
Загрузка недоступна: ресурсы заняты
Этот источник разрешает только просмотр. Для импорта выберите другой источник.
Минимальный масштаб
Сервер ограничивает загрузки. Повторите попытку позже.
Область слишком велика: уменьшите детализацию или приблизьте карту.
Максимальный масштаб
Приостановлено
Скачать область
Ограничение источника
Идёт загрузка
Загрузка завершена
Скачать / продолжить
Отображаемая область
Загрузка прервана. Сохранённые тайлы остаются на карте памяти.
Не удалось сохранить. Сохранённые тайлы остаются на карте памяти.
Назначить начальную позицию здесь
Обзор\\nDNS\\nОтвет сети\\nПорты TCP\\nHTTP и TLS
IP-адрес или имя узла
Ввод
Удалить
Меню и консоли''',
'ar':'''التحليل جارٍ.
مسح المخرجات
مسح المخرجات · Ctrl+L
معلومات
إخفاء التحديد أو إظهاره
تهيئة هذا المجلد باستخدام Git
تحديد متعدد
عدم إظهار الملفات المخفية
الخروج من التحديد المتعدد
إظهار الملفات المخفية
لم يُستلم أي إدخال
إدخالات غير معروفة
يد التحكم
لم يُحفظ الإعداد
التنزيل غير متاح: الموارد مشغولة
يسمح هذا المصدر بالعرض فقط. اختر مصدرًا آخر للاستيراد.
التكبير الأدنى
يحدّ الخادم من التنزيلات. حاول لاحقًا.
المنطقة كبيرة جدًا: قلّل التفاصيل أو قرّب الخريطة.
التكبير الأقصى
متوقف مؤقتًا
تنزيل المنطقة
حدّ المصدر
التنزيل جارٍ
اكتمل التنزيل
تنزيل / استئناف
المنطقة المعروضة
انقطع التنزيل. تُحتفظ بالمربعات المحفوظة.
تعذّر الحفظ. تُحتفظ بالمربعات المحفوظة.
تعيين موضع البداية هنا
نظرة عامة\\nDNS\\nاستجابة الشبكة\\nمنافذ TCP\\nHTTP وTLS
عنوان IP أو اسم المضيف
إدخال
حذف
القوائم ووحدات التحكم''',
'zh-CN':'''正在分析。
清空输出
清空输出 · Ctrl+L
信息
隐藏或显示所选项
在此文件夹中初始化 Git
多选
不显示隐藏文件
退出多选
显示隐藏文件
尚未收到输入
无法识别输入
手柄
设置未保存
无法下载：资源正忙
此来源仅允许浏览。请选择其他来源进行导入。
最小缩放级别
服务器正在限制下载。请稍后重试。
区域过大：请降低细节级别或放大地图。
最大缩放级别
已暂停
下载区域
来源限制
正在下载
下载完成
下载 / 继续
当前显示区域
下载中断。已保存的瓦片会保留。
无法保存。已保存的瓦片会保留。
在此设置起始位置
概览\\nDNS\\n网络响应\\nTCP 端口\\nHTTP 和 TLS
IP 地址或主机名
回车
删除
菜单和控制台''',
'ja-JP':'''解析中です。
出力を消去
出力を消去 · Ctrl+L
情報
選択項目を非表示 / 表示
このフォルダーで Git を初期化
複数選択
隠しファイルを表示しない
複数選択を終了
隠しファイルを表示
入力を受信していません
入力を認識できません
コントローラー
設定を保存できませんでした
ダウンロードできません：リソース使用中
この配信元では閲覧のみ可能です。インポートには別の配信元を選択してください。
最小ズーム
サーバーがダウンロードを制限しています。後で再試行してください。
範囲が広すぎます：詳細度を下げるか、地図を拡大してください。
最大ズーム
一時停止中
範囲をダウンロード
配信元の上限
ダウンロード中
ダウンロード完了
ダウンロード / 再開
表示範囲
ダウンロードが中断しました。保存済みタイルは保持されます。
保存できません。保存済みタイルは保持されます。
ここを初期位置に設定
概要\\nDNS\\nネットワーク応答\\nTCP ポート\\nHTTP と TLS
IP アドレスまたはホスト名
Enter
Delete
メニューとコンソール''',
'ko-KR':'''분석 중입니다.
출력 지우기
출력 지우기 · Ctrl+L
정보
선택 항목 숨기기 / 표시
이 폴더에서 Git 초기화
다중 선택
숨김 파일 표시 안 함
다중 선택 종료
숨김 파일 표시
수신된 입력 없음
입력을 인식할 수 없음
컨트롤러
설정이 저장되지 않음
다운로드 불가: 자원 사용 중
이 소스는 보기만 허용합니다. 가져오려면 다른 소스를 선택하세요.
최소 확대 수준
서버가 다운로드를 제한합니다. 나중에 다시 시도하세요.
영역이 너무 큽니다: 세부 수준을 낮추거나 지도를 확대하세요.
최대 확대 수준
일시 정지됨
영역 다운로드
소스 제한
다운로드 중
다운로드 완료
다운로드 / 계속
표시 영역
다운로드가 중단되었습니다. 저장된 타일은 유지됩니다.
저장할 수 없습니다. 저장된 타일은 유지됩니다.
여기를 초기 위치로 설정
개요\\nDNS\\n네트워크 응답\\nTCP 포트\\nHTTP 및 TLS
IP 주소 또는 호스트 이름
Enter
Delete
메뉴와 콘솔'''
}

def apply():
    delta={'fr-FR':dict(zip(KEYS,KEYS))}
    for code,text in ROWS.items():
        values=[s.replace('\\n','\n') for s in text.splitlines()]
        assert len(values)==len(KEYS),(code,len(values),len(KEYS))
        delta[code]=dict(zip(KEYS,values))
    assert len(delta)==14
    for code,translations in delta.items():
        path=SKETCH/'sdcard/System/languages'/(code+'.json')
        pack=json.loads(path.read_text(encoding='utf8'))
        pack['translations'].update(translations)
        path.write_text(json.dumps(pack,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    (HERE/'reference-delta-0823.json').write_text(json.dumps(KEYS,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    (HERE/'delta-ui-0823.json').write_text(json.dumps(delta,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print('Applied',len(KEYS),'keys in',len(delta),'languages.')

if __name__=='__main__':apply()
