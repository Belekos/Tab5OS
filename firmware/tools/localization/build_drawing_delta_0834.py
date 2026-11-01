from pathlib import Path
import json
HERE=Path(__file__).resolve().parent
KEYS=r'''Dessin
Dureté
Forme du pinceau
Rond
Carré
Barre horizontale
Barre verticale
Barre diagonale /
Barre diagonale \
Main
Déformation locale
Force
Pousser
Gonfler
Pincer
Depuis le centre
Cercle
Options de l’outil
Ajuster à la fenêtre
Taille réelle · 100 %
Zoom avant
Zoom arrière
Couleur de premier plan
Couleur d’arrière-plan
Permuter les couleurs
Dureté : bord du trait. Diffusion : dispersion des points.
Déplacer la vue sans modifier l’image.
Tracer depuis deux coins opposés, ou depuis le centre.
Importer une image
Exporter en JPEG
Exporter une image JPEG
Image JPEG exportée
Formats pris en charge : BMP, T5I et JPEG
Enregistrer dans /Images avec l’extension .t5i, .bmp, .jpg ou .jpeg
Utiliser /Images/nom.t5i, /Images/nom.bmp ou /Images/nom.jpg
T5I conserve les calques. BMP et JPEG exportent l’image aplatie sur fond blanc.
JPEG réduit à l’import ; fichier source conservé
La déformation locale agit sur le calque actif et respecte la sélection.
Longueur
Angle'''.splitlines()
ROWS={}
ROWS['en-US']='''Drawing
Hardness
Brush shape
Round
Square
Horizontal bar
Vertical bar
Diagonal bar /
Diagonal bar \\
Hand
Local warp
Strength
Push
Inflate
Pinch
From centre
Circle
Tool options
Fit to window
Actual size · 100 %
Zoom in
Zoom out
Foreground colour
Background colour
Swap colours
Hardness: stroke edge. Diffusion: scattered dots.
Move the view without changing the image.
Draw from two opposite corners, or from the centre.
Import an image
Export as JPEG
Export a JPEG image
JPEG image exported
Supported formats: BMP, T5I and JPEG
Save in /Images with the .t5i, .bmp, .jpg or .jpeg extension
Use /Images/name.t5i, /Images/name.bmp or /Images/name.jpg
T5I preserves layers. BMP and JPEG export the flattened image on white.
JPEG reduced on import; source file preserved
Local warp affects the active layer and respects the selection.
Length
Angle'''.splitlines()
ROWS['de-DE']='''Zeichnen
Härte
Pinselform
Rund
Quadratisch
Waagerechter Balken
Senkrechter Balken
Diagonaler Balken /
Diagonaler Balken \\
Hand
Lokale Verformung
Stärke
Schieben
Aufblähen
Zusammenziehen
Vom Mittelpunkt
Kreis
Werkzeugoptionen
An Fenster anpassen
Originalgröße · 100 %
Vergrößern
Verkleinern
Vordergrundfarbe
Hintergrundfarbe
Farben tauschen
Härte: Rand des Strichs. Streuung: Verteilung der Punkte.
Ansicht verschieben, ohne das Bild zu ändern.
Von zwei gegenüberliegenden Ecken oder vom Mittelpunkt zeichnen.
Bild importieren
Als JPEG exportieren
JPEG-Bild exportieren
JPEG-Bild exportiert
Unterstützte Formate: BMP, T5I und JPEG
In /Images mit der Endung .t5i, .bmp, .jpg oder .jpeg speichern
/Images/name.t5i, /Images/name.bmp oder /Images/name.jpg verwenden
T5I erhält Ebenen. BMP und JPEG exportieren das zusammengefügte Bild auf Weiß.
JPEG beim Import verkleinert; Quelldatei erhalten
Die lokale Verformung wirkt auf die aktive Ebene und beachtet die Auswahl.
Länge
Winkel'''.splitlines()
ROWS['es-ES']='''Dibujo
Dureza
Forma del pincel
Redondo
Cuadrado
Barra horizontal
Barra vertical
Barra diagonal /
Barra diagonal \\
Mano
Deformación local
Intensidad
Empujar
Inflar
Contraer
Desde el centro
Círculo
Opciones de la herramienta
Ajustar a la ventana
Tamaño real · 100 %
Acercar
Alejar
Color de primer plano
Color de fondo
Intercambiar colores
Dureza: borde del trazo. Difusión: dispersión de puntos.
Mover la vista sin modificar la imagen.
Trazar desde dos esquinas opuestas o desde el centro.
Importar una imagen
Exportar como JPEG
Exportar una imagen JPEG
Imagen JPEG exportada
Formatos compatibles: BMP, T5I y JPEG
Guardar en /Images con la extensión .t5i, .bmp, .jpg o .jpeg
Usar /Images/nombre.t5i, /Images/nombre.bmp o /Images/nombre.jpg
T5I conserva las capas. BMP y JPEG exportan la imagen aplanada sobre fondo blanco.
JPEG reducido al importar; archivo original conservado
La deformación local actúa sobre la capa activa y respeta la selección.
Longitud
Ángulo'''.splitlines()
ROWS['it-IT']='''Disegno
Durezza
Forma del pennello
Rotondo
Quadrato
Barra orizzontale
Barra verticale
Barra diagonale /
Barra diagonale \\
Mano
Deformazione locale
Intensità
Spingi
Gonfia
Restringi
Dal centro
Cerchio
Opzioni dello strumento
Adatta alla finestra
Dimensione reale · 100 %
Ingrandisci
Riduci
Colore in primo piano
Colore di sfondo
Scambia i colori
Durezza: bordo del tratto. Diffusione: dispersione dei punti.
Sposta la vista senza modificare l’immagine.
Disegna da due angoli opposti oppure dal centro.
Importa un’immagine
Esporta in JPEG
Esporta un’immagine JPEG
Immagine JPEG esportata
Formati supportati: BMP, T5I e JPEG
Salva in /Images con estensione .t5i, .bmp, .jpg o .jpeg
Usa /Images/nome.t5i, /Images/nome.bmp o /Images/nome.jpg
T5I conserva i livelli. BMP e JPEG esportano l’immagine unita su sfondo bianco.
JPEG ridotto all’importazione; file originale conservato
La deformazione locale agisce sul livello attivo e rispetta la selezione.
Lunghezza
Angolo'''.splitlines()
ROWS['nl-NL']='''Tekenen
Hardheid
Penseelvorm
Rond
Vierkant
Horizontale balk
Verticale balk
Diagonale balk /
Diagonale balk \\
Hand
Plaatselijke vervorming
Sterkte
Duwen
Opblazen
Samentrekken
Vanuit het midden
Cirkel
Gereedschapsopties
Aan venster aanpassen
Ware grootte · 100 %
Inzoomen
Uitzoomen
Voorgrondkleur
Achtergrondkleur
Kleuren verwisselen
Hardheid: rand van de streek. Spreiding: verdeling van stippen.
Verplaats de weergave zonder de afbeelding te wijzigen.
Teken vanuit twee tegenoverliggende hoeken of vanuit het midden.
Afbeelding importeren
Als JPEG exporteren
JPEG-afbeelding exporteren
JPEG-afbeelding geëxporteerd
Ondersteunde formaten: BMP, T5I en JPEG
Opslaan in /Images met de extensie .t5i, .bmp, .jpg of .jpeg
Gebruik /Images/naam.t5i, /Images/naam.bmp of /Images/naam.jpg
T5I bewaart lagen. BMP en JPEG exporteren de samengevoegde afbeelding op wit.
JPEG verkleind bij import; bronbestand behouden
Plaatselijke vervorming werkt op de actieve laag en houdt rekening met de selectie.
Lengte
Hoek'''.splitlines()
ROWS['pt-BR']='''Desenho
Dureza
Forma do pincel
Redondo
Quadrado
Barra horizontal
Barra vertical
Barra diagonal /
Barra diagonal \\
Mão
Deformação local
Intensidade
Empurrar
Inflar
Contrair
A partir do centro
Círculo
Opções da ferramenta
Ajustar à janela
Tamanho real · 100 %
Ampliar
Reduzir
Cor de primeiro plano
Cor de fundo
Trocar as cores
Dureza: borda do traço. Difusão: dispersão dos pontos.
Mover a visualização sem modificar a imagem.
Desenhar a partir de dois cantos opostos ou do centro.
Importar uma imagem
Exportar como JPEG
Exportar uma imagem JPEG
Imagem JPEG exportada
Formatos compatíveis: BMP, T5I e JPEG
Salvar em /Images com a extensão .t5i, .bmp, .jpg ou .jpeg
Usar /Images/nome.t5i, /Images/nome.bmp ou /Images/nome.jpg
T5I preserva as camadas. BMP e JPEG exportam a imagem mesclada sobre fundo branco.
JPEG reduzido na importação; arquivo de origem preservado
A deformação local atua na camada ativa e respeita a seleção.
Comprimento
Ângulo'''.splitlines()
ROWS['pt-PT']='''Desenho
Dureza
Forma do pincel
Redondo
Quadrado
Barra horizontal
Barra vertical
Barra diagonal /
Barra diagonal \\
Mão
Deformação local
Intensidade
Empurrar
Insuflar
Contrair
A partir do centro
Círculo
Opções da ferramenta
Ajustar à janela
Tamanho real · 100 %
Ampliar
Reduzir
Cor de primeiro plano
Cor de fundo
Trocar as cores
Dureza: bordo do traço. Difusão: dispersão dos pontos.
Mover a vista sem modificar a imagem.
Desenhar a partir de dois cantos opostos ou do centro.
Importar uma imagem
Exportar como JPEG
Exportar uma imagem JPEG
Imagem JPEG exportada
Formatos suportados: BMP, T5I e JPEG
Guardar em /Images com a extensão .t5i, .bmp, .jpg ou .jpeg
Usar /Images/nome.t5i, /Images/nome.bmp ou /Images/nome.jpg
T5I preserva as camadas. BMP e JPEG exportam a imagem fundida sobre fundo branco.
JPEG reduzido na importação; ficheiro de origem preservado
A deformação local atua na camada ativa e respeita a seleção.
Comprimento
Ângulo'''.splitlines()
ROWS['id-ID']='''Menggambar
Kekerasan
Bentuk kuas
Bulat
Persegi
Batang mendatar
Batang tegak
Batang diagonal /
Batang diagonal \\
Tangan
Deformasi lokal
Kekuatan
Dorong
Gembungkan
Ciutkan
Dari pusat
Lingkaran
Opsi alat
Sesuaikan ke jendela
Ukuran asli · 100 %
Perbesar
Perkecil
Warna latar depan
Warna latar belakang
Tukar warna
Kekerasan: tepi goresan. Difusi: sebaran titik.
Geser tampilan tanpa mengubah gambar.
Gambar dari dua sudut berlawanan, atau dari pusat.
Impor gambar
Ekspor sebagai JPEG
Ekspor gambar JPEG
Gambar JPEG diekspor
Format yang didukung: BMP, T5I dan JPEG
Simpan di /Images dengan ekstensi .t5i, .bmp, .jpg atau .jpeg
Gunakan /Images/nama.t5i, /Images/nama.bmp atau /Images/nama.jpg
T5I mempertahankan lapisan. BMP dan JPEG mengekspor gambar gabungan berlatar putih.
JPEG diperkecil saat diimpor; berkas sumber dipertahankan
Deformasi lokal bekerja pada lapisan aktif dan mengikuti seleksi.
Panjang
Sudut'''.splitlines()
ROWS['ru-RU']='''Рисование
Жёсткость
Форма кисти
Круглая
Квадратная
Горизонтальная полоса
Вертикальная полоса
Диагональная полоса /
Диагональная полоса \\
Рука
Локальная деформация
Сила
Сдвиг
Раздувание
Сжатие
Из центра
Круг
Параметры инструмента
Вписать в окно
Истинный размер · 100 %
Приблизить
Отдалить
Цвет переднего плана
Цвет фона
Поменять цвета
Жёсткость: край штриха. Рассеивание: разброс точек.
Перемещайте вид без изменения изображения.
Рисуйте от двух противоположных углов или от центра.
Импорт изображения
Экспорт в JPEG
Экспорт изображения JPEG
Изображение JPEG экспортировано
Форматы: BMP, T5I и JPEG
Сохраните в /Images с расширением .t5i, .bmp, .jpg или .jpeg
Используйте /Images/name.t5i, /Images/name.bmp или /Images/name.jpg
T5I сохраняет слои. BMP и JPEG экспортируют сведённое изображение на белом фоне.
JPEG уменьшен при импорте; исходный файл сохранён
Локальная деформация действует на активный слой в пределах выделения.
Длина
Угол'''.splitlines()
ROWS['ar']='''الرسم
الصلابة
شكل الفرشاة
دائري
مربع
شريط أفقي
شريط عمودي
شريط مائل /
شريط مائل \\
اليد
تشويه موضعي
القوة
دفع
نفخ
تقليص
من المركز
دائرة
خيارات الأداة
ملاءمة النافذة
الحجم الفعلي · 100 %
تكبير
تصغير
لون المقدمة
لون الخلفية
تبديل اللونين
الصلابة: حافة الخط. الانتشار: توزيع النقاط.
حرّك العرض دون تعديل الصورة.
ارسم من زاويتين متقابلتين أو من المركز.
استيراد صورة
تصدير بصيغة JPEG
تصدير صورة JPEG
تم تصدير صورة JPEG
الصيغ المدعومة: BMP وT5I وJPEG
احفظ في /Images بامتداد .t5i أو .bmp أو .jpg أو .jpeg
استخدم /Images/name.t5i أو /Images/name.bmp أو /Images/name.jpg
يحفظ T5I الطبقات. يصدّر BMP وJPEG الصورة المدمجة على خلفية بيضاء.
تم تصغير JPEG عند الاستيراد؛ الملف الأصلي محفوظ
يؤثر التشويه الموضعي في الطبقة النشطة ضمن حدود التحديد.
الطول
الزاوية'''.splitlines()
ROWS['zh-CN']='''绘画
硬度
画笔形状
圆形
方形
水平条形
垂直条形
斜条形 /
斜条形 \\
抓手
局部变形
强度
推动
膨胀
收缩
从中心绘制
正圆
工具选项
适合窗口
实际大小 · 100 %
放大
缩小
前景色
背景色
交换颜色
硬度控制笔触边缘，扩散控制点的散布。
移动视图，不修改图像。
从两个对角或中心开始绘制。
导入图像
导出为 JPEG
导出 JPEG 图像
JPEG 图像已导出
支持的格式：BMP、T5I 和 JPEG
保存在 /Images 中，扩展名为 .t5i、.bmp、.jpg 或 .jpeg
使用 /Images/name.t5i、/Images/name.bmp 或 /Images/name.jpg
T5I 保留图层。BMP 和 JPEG 导出白色背景的合并图像。
导入时已缩小 JPEG；源文件保持不变
局部变形作用于当前图层，并限制在选区内。
长度
角度'''.splitlines()
ROWS['ja-JP']='''お絵描き
硬さ
ブラシの形
丸
四角
横棒
縦棒
斜めの棒 /
斜めの棒 \\
手のひら
部分変形
強さ
押す
膨らませる
縮める
中心から
正円
ツールの設定
ウィンドウに合わせる
実際のサイズ · 100 %
拡大
縮小
描画色
背景色
色を入れ替える
硬さは線の縁、拡散は点の散らばりを調整します。
画像を変更せずに表示を移動します。
対角の2点、または中心から描きます。
画像を読み込む
JPEGで書き出す
JPEG画像を書き出す
JPEG画像を書き出しました
対応形式：BMP、T5I、JPEG
拡張子 .t5i、.bmp、.jpg、.jpeg のいずれかで /Images に保存
/Images/name.t5i、/Images/name.bmp、/Images/name.jpg のいずれかを使用
T5Iはレイヤーを保持します。BMPとJPEGは白背景に統合した画像を書き出します。
読み込み時にJPEGを縮小しました。元ファイルは変更していません
部分変形は選択範囲内の現在のレイヤーに適用されます。
長さ
角度'''.splitlines()
ROWS['ko-KR']='''그림
경도
브러시 모양
원형
사각형
가로 막대
세로 막대
대각선 막대 /
대각선 막대 \\
손
부분 변형
강도
밀기
부풀리기
오므리기
중심에서
원
도구 옵션
창에 맞추기
실제 크기 · 100 %
확대
축소
전경색
배경색
색상 바꾸기
경도는 획의 가장자리를, 분산은 점의 흩어짐을 조절합니다.
이미지를 수정하지 않고 화면을 이동합니다.
마주 보는 두 모서리 또는 중심에서 그립니다.
이미지 가져오기
JPEG로 내보내기
JPEG 이미지 내보내기
JPEG 이미지를 내보냈습니다
지원 형식: BMP, T5I 및 JPEG
/Images에 .t5i, .bmp, .jpg 또는 .jpeg 확장자로 저장
/Images/name.t5i, /Images/name.bmp 또는 /Images/name.jpg 사용
T5I는 레이어를 유지합니다. BMP와 JPEG는 흰 배경으로 합친 이미지를 내보냅니다.
가져올 때 JPEG를 축소했습니다. 원본 파일은 유지됩니다
부분 변형은 활성 레이어에 적용되며 선택 영역을 벗어나지 않습니다.
길이
각도'''.splitlines()
ROWS['fr-FR']=KEYS
for locale,values in ROWS.items():
 assert len(values)==len(KEYS),(locale,len(values),len(KEYS))
DELTA={locale:dict(zip(KEYS,values)) for locale,values in ROWS.items()}
if __name__=='__main__':
 (HERE/'drawing-ui-delta-0834.json').write_text(json.dumps(DELTA,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 (HERE/'drawing-ui-en-0834.json').write_text(json.dumps(DELTA['en-US'],ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print('UI staged',len(KEYS),'keys',len(DELTA),'languages')
