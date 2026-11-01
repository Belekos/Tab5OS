#!/usr/bin/env python3
"""Reviewed 0.8.35 UI additions; each row is translated in all 14 languages."""
from pathlib import Path
import json

HERE = Path(__file__).resolve().parent
KEYS = '''Transformation
Afficher les options
Masquer les options
Visualisateur
Autres applications…
Ouvrir avec le visualisateur
Ouvrir avec Dessin'''.splitlines()
ROWS = {
    'fr-FR': KEYS,
    'en-US': '''Transform
Show options
Hide options
Viewer
Other applications…
Open with Viewer
Open with Drawing'''.splitlines(),
    'de-DE': '''Transformation
Optionen anzeigen
Optionen ausblenden
Bildbetrachter
Weitere Anwendungen…
Mit Bildbetrachter öffnen
Mit Zeichnen öffnen'''.splitlines(),
    'es-ES': '''Transformación
Mostrar opciones
Ocultar opciones
Visor
Otras aplicaciones…
Abrir con el visor
Abrir con Dibujo'''.splitlines(),
    'it-IT': '''Trasformazione
Mostra opzioni
Nascondi opzioni
Visualizzatore
Altre applicazioni…
Apri con il visualizzatore
Apri con Disegno'''.splitlines(),
    'nl-NL': '''Transformatie
Opties tonen
Opties verbergen
Afbeeldingsviewer
Andere toepassingen…
Openen met afbeeldingsviewer
Openen met Tekenen'''.splitlines(),
    'pt-BR': '''Transformação
Mostrar opções
Ocultar opções
Visualizador
Outros aplicativos…
Abrir com o visualizador
Abrir com Desenho'''.splitlines(),
    'pt-PT': '''Transformação
Mostrar opções
Ocultar opções
Visualizador
Outras aplicações…
Abrir com o visualizador
Abrir com Desenho'''.splitlines(),
    'id-ID': '''Transformasi
Tampilkan opsi
Sembunyikan opsi
Penampil gambar
Aplikasi lain…
Buka dengan penampil gambar
Buka dengan Menggambar'''.splitlines(),
    'ru-RU': '''Преобразование
Показать параметры
Скрыть параметры
Просмотрщик
Другие приложения…
Открыть в просмотрщике
Открыть в Рисовании'''.splitlines(),
    'zh-CN': '''变换
显示选项
隐藏选项
查看器
其他应用…
用查看器打开
用绘画打开'''.splitlines(),
    'ja-JP': '''変形
オプションを表示
オプションを非表示
画像ビューアー
その他のアプリ…
画像ビューアーで開く
お絵描きで開く'''.splitlines(),
    'ko-KR': '''변환
옵션 표시
옵션 숨기기
이미지 뷰어
다른 앱…
이미지 뷰어로 열기
그림으로 열기'''.splitlines(),
    'ar': '''تحويل
إظهار الخيارات
إخفاء الخيارات
عارض الصور
تطبيقات أخرى…
فتح باستخدام عارض الصور
فتح باستخدام الرسم'''.splitlines(),
}

FORMAT_KEYS = '''Dimensions/calques hors limite (8192, moins de 8 Mio de pixels).
BMP non pris en charge (24/32 bits non compresse, <=8192).
Image PNG
Import PNG annule; image conservee.
Lecture PNG impossible.
Memoire insuffisante pour le decodeur PNG.
PNG incomplet ou invalide; image conservee.
PNG trop volumineux (16 Mio maximum).
WebP incomplet ou invalide; image conservee.
Import WebP annule; image conservee.
Image WebP'''.splitlines()
FORMAT_ROWS = {
    'fr-FR': '''Dimensions ou calques hors limite (8192, moins de 8 Mio de pixels).
BMP non pris en charge (24/32 bits non compressé, ≤8192).
Image PNG
Import PNG annulé ; image conservée.
Lecture PNG impossible.
Mémoire insuffisante pour le décodeur PNG.
PNG incomplet ou invalide ; image conservée.
PNG trop volumineux (16 Mio maximum).
WebP incomplet ou invalide ; image conservée.
Import WebP annulé ; image conservée.
Image WebP'''.splitlines(),
    'en-US': '''Dimensions or layers exceed limits (8192, less than 8 MiB of pixels).
Unsupported BMP (uncompressed 24/32-bit, ≤8192).
PNG image
PNG import cancelled; image preserved.
Cannot read PNG.
Not enough memory for the PNG decoder.
Incomplete or invalid PNG; image preserved.
PNG too large (16 MiB maximum).
Incomplete or invalid WebP; image preserved.
WebP import cancelled; image preserved.
WebP image'''.splitlines(),
    'de-DE': '''Maße oder Ebenen überschreiten die Grenzen (8192, weniger als 8 MiB Pixeldaten).
BMP nicht unterstützt (24/32 Bit, unkomprimiert, ≤8192).
PNG-Bild
PNG-Import abgebrochen; Bild erhalten.
PNG kann nicht gelesen werden.
Nicht genug Speicher für den PNG-Decoder.
PNG unvollständig oder ungültig; Bild erhalten.
PNG zu groß (maximal 16 MiB).
WebP unvollständig oder ungültig; Bild erhalten.
WebP-Import abgebrochen; Bild erhalten.
WebP-Bild'''.splitlines(),
    'es-ES': '''Dimensiones o capas fuera de los límites (8192, menos de 8 MiB de píxeles).
BMP no compatible (24/32 bits sin comprimir, ≤8192).
Imagen PNG
Importación PNG cancelada; imagen conservada.
No se puede leer el PNG.
Memoria insuficiente para el decodificador PNG.
PNG incompleto o no válido; imagen conservada.
PNG demasiado grande (máximo 16 MiB).
WebP incompleto o no válido; imagen conservada.
Importación WebP cancelada; imagen conservada.
Imagen WebP'''.splitlines(),
    'it-IT': '''Dimensioni o livelli oltre i limiti (8192, meno di 8 MiB di pixel).
BMP non supportato (24/32 bit non compresso, ≤8192).
Immagine PNG
Importazione PNG annullata; immagine conservata.
Impossibile leggere il PNG.
Memoria insufficiente per il decodificatore PNG.
PNG incompleto o non valido; immagine conservata.
PNG troppo grande (massimo 16 MiB).
WebP incompleto o non valido; immagine conservata.
Importazione WebP annullata; immagine conservata.
Immagine WebP'''.splitlines(),
    'nl-NL': '''Afmetingen of lagen buiten de grenzen (8192, minder dan 8 MiB pixeldata).
Niet-ondersteunde BMP (ongecomprimeerd 24/32-bit, ≤8192).
PNG-afbeelding
PNG-import geannuleerd; afbeelding behouden.
PNG kan niet worden gelezen.
Onvoldoende geheugen voor de PNG-decoder.
PNG onvolledig of ongeldig; afbeelding behouden.
PNG te groot (maximaal 16 MiB).
WebP onvolledig of ongeldig; afbeelding behouden.
WebP-import geannuleerd; afbeelding behouden.
WebP-afbeelding'''.splitlines(),
    'pt-BR': '''Dimensões ou camadas fora dos limites (8192, menos de 8 MiB de pixels).
BMP não compatível (24/32 bits sem compressão, ≤8192).
Imagem PNG
Importação PNG cancelada; imagem preservada.
Não foi possível ler o PNG.
Memória insuficiente para o decodificador PNG.
PNG incompleto ou inválido; imagem preservada.
PNG muito grande (máximo de 16 MiB).
WebP incompleto ou inválido; imagem preservada.
Importação WebP cancelada; imagem preservada.
Imagem WebP'''.splitlines(),
    'pt-PT': '''Dimensões ou camadas fora dos limites (8192, menos de 8 MiB de píxeis).
BMP não suportado (24/32 bits sem compressão, ≤8192).
Imagem PNG
Importação PNG cancelada; imagem preservada.
Não foi possível ler o PNG.
Memória insuficiente para o descodificador PNG.
PNG incompleto ou inválido; imagem preservada.
PNG demasiado grande (máximo de 16 MiB).
WebP incompleto ou inválido; imagem preservada.
Importação WebP cancelada; imagem preservada.
Imagem WebP'''.splitlines(),
    'id-ID': '''Dimensi atau lapisan melampaui batas (8192, data piksel kurang dari 8 MiB).
BMP tidak didukung (24/32-bit tanpa kompresi, ≤8192).
Gambar PNG
Impor PNG dibatalkan; gambar dipertahankan.
Tidak dapat membaca PNG.
Memori tidak cukup untuk dekoder PNG.
PNG tidak lengkap atau tidak valid; gambar dipertahankan.
PNG terlalu besar (maksimal 16 MiB).
WebP tidak lengkap atau tidak valid; gambar dipertahankan.
Impor WebP dibatalkan; gambar dipertahankan.
Gambar WebP'''.splitlines(),
    'ru-RU': '''Превышен предел размеров или слоёв (8192, менее 8 МиБ пикселей).
BMP не поддерживается (24/32 бита без сжатия, ≤8192).
Изобр. PNG
Импорт PNG отменён; изображение сохранено.
Не удалось прочитать PNG.
Недостаточно памяти для декодера PNG.
PNG неполный или неверный; изображение сохранено.
PNG слишком велик (не более 16 МиБ).
WebP неполный или неверный; изображение сохранено.
Импорт WebP отменён; изображение сохранено.
Изобр. WebP'''.splitlines(),
    'zh-CN': '''尺寸或图层超出限制（8192，像素数据小于 8 MiB）。
不支持此 BMP（须为未压缩的 24/32 位，≤8192）。
PNG 图像
已取消 PNG 导入；图像已保留。
无法读取 PNG。
内存不足，无法运行 PNG 解码器。
PNG 不完整或无效；图像已保留。
PNG 过大（最大 16 MiB）。
WebP 不完整或无效；图像已保留。
已取消 WebP 导入；图像已保留。
WebP 图像'''.splitlines(),
    'ja-JP': '''サイズまたはレイヤー数が上限を超えています（8192、画素データは 8 MiB 未満）。
非対応の BMP です（非圧縮の 24/32 ビット、8192 以下）。
PNG 画像
PNG の読み込みを中止しました。画像は保持されます。
PNG を読み込めません。
PNG デコーダーのメモリーが不足しています。
PNG が不完全または無効です。画像は保持されます。
PNG が大きすぎます（最大 16 MiB）。
WebP が不完全または無効です。画像は保持されます。
WebP の読み込みを中止しました。画像は保持されます。
WebP 画像'''.splitlines(),
    'ko-KR': '''크기 또는 레이어 수가 한도를 초과합니다(8192, 픽셀 데이터 8 MiB 미만).
지원하지 않는 BMP입니다(비압축 24/32비트, 8192 이하).
PNG 이미지
PNG 가져오기를 취소했습니다. 이미지는 유지됩니다.
PNG를 읽을 수 없습니다.
PNG 디코더에 사용할 메모리가 부족합니다.
PNG가 불완전하거나 올바르지 않습니다. 이미지는 유지됩니다.
PNG가 너무 큽니다(최대 16 MiB).
WebP가 불완전하거나 올바르지 않습니다. 이미지는 유지됩니다.
WebP 가져오기를 취소했습니다. 이미지는 유지됩니다.
WebP 이미지'''.splitlines(),
    'ar': '''الأبعاد أو الطبقات تتجاوز الحد (8192، وأقل من 8 ميبيبايت من بيانات البكسل).
صيغة BMP غير مدعومة (24/32 بت غير مضغوط، ≤8192).
صورة PNG
أُلغي استيراد PNG؛ حُفظت الصورة.
تعذّرت قراءة PNG.
الذاكرة غير كافية لفك ترميز PNG.
ملف PNG غير مكتمل أو غير صالح؛ حُفظت الصورة.
ملف PNG كبير جداً (16 ميبيبايت كحد أقصى).
ملف WebP غير مكتمل أو غير صالح؛ حُفظت الصورة.
أُلغي استيراد WebP؛ حُفظت الصورة.
صورة WebP'''.splitlines(),
}

USAGE_KEYS = '''Cliquer dans l’image puis écrire. Entrée valide ; Échap annule.
Formats pris en charge : BMP, T5I, JPEG, PNG et WebP
Dimensions : 1 à 8192 pixels'''.splitlines()
USAGE_ROWS = {
    'fr-FR': USAGE_KEYS,
    'en-US': '''Click the image, then type. Enter confirms; Esc cancels.
Supported formats: BMP, T5I, JPEG, PNG and WebP
Dimensions: 1 to 8192 pixels'''.splitlines(),
    'de-DE': '''Ins Bild klicken und schreiben. Eingabe bestätigt; Esc bricht ab.
Unterstützte Formate: BMP, T5I, JPEG, PNG und WebP
Maße: 1 bis 8192 Pixel'''.splitlines(),
    'es-ES': '''Haz clic en la imagen y escribe. Intro confirma; Esc cancela.
Formatos compatibles: BMP, T5I, JPEG, PNG y WebP
Dimensiones: de 1 a 8192 píxeles'''.splitlines(),
    'it-IT': '''Fai clic sull’immagine e scrivi. Invio conferma; Esc annulla.
Formati supportati: BMP, T5I, JPEG, PNG e WebP
Dimensioni: da 1 a 8192 pixel'''.splitlines(),
    'nl-NL': '''Klik op de afbeelding en typ. Enter bevestigt; Esc annuleert.
Ondersteunde formaten: BMP, T5I, JPEG, PNG en WebP
Afmetingen: 1 tot 8192 pixels'''.splitlines(),
    'pt-BR': '''Clique na imagem e digite. Enter confirma; Esc cancela.
Formatos compatíveis: BMP, T5I, JPEG, PNG e WebP
Dimensões: de 1 a 8192 pixels'''.splitlines(),
    'pt-PT': '''Clique na imagem e escreva. Enter confirma; Esc cancela.
Formatos suportados: BMP, T5I, JPEG, PNG e WebP
Dimensões: de 1 a 8192 píxeis'''.splitlines(),
    'id-ID': '''Klik gambar, lalu ketik. Enter mengonfirmasi; Esc membatalkan.
Format yang didukung: BMP, T5I, JPEG, PNG, dan WebP
Dimensi: 1 hingga 8192 piksel'''.splitlines(),
    'ru-RU': '''Щёлкните по изображению и введите текст. Enter — принять; Esc — отменить.
Поддерживаемые форматы: BMP, T5I, JPEG, PNG и WebP
Размеры: от 1 до 8192 пикселей'''.splitlines(),
    'zh-CN': '''点击图像后输入文字。按回车确认；按 Esc 取消。
支持的格式：BMP、T5I、JPEG、PNG 和 WebP
尺寸：1 至 8192 像素'''.splitlines(),
    'ja-JP': '''画像をクリックして文字を入力します。Enter で確定、Esc で取り消します。
対応形式：BMP、T5I、JPEG、PNG、WebP
サイズ：1～8192 ピクセル'''.splitlines(),
    'ko-KR': '''이미지를 클릭한 다음 입력하세요. Enter로 확인하고 Esc로 취소합니다.
지원 형식: BMP, T5I, JPEG, PNG, WebP
크기: 1~8192픽셀'''.splitlines(),
    'ar': '''انقر على الصورة ثم اكتب. يؤكد Enter ويلغي Esc.
الصيغ المدعومة: BMP وT5I وJPEG وPNG وWebP
الأبعاد: من 1 إلى 8192 بكسل'''.splitlines(),
}


LAYER_LABEL = {
    'fr-FR': 'Afficher les calques', 'en-US': 'Show layers',
    'de-DE': 'Ebenen anzeigen', 'es-ES': 'Mostrar capas',
    'it-IT': 'Mostra livelli', 'nl-NL': 'Lagen tonen',
    'pt-BR': 'Mostrar camadas', 'pt-PT': 'Mostrar camadas',
    'id-ID': 'Tampilkan lapisan', 'ru-RU': 'Показать слои',
    'zh-CN': '显示图层', 'ja-JP': 'レイヤーを表示',
    'ko-KR': '레이어 표시', 'ar': 'إظهار الطبقات',
}


def main():
    assert len(ROWS) == 14
    assert all(len(values) == len(KEYS) for values in ROWS.values())
    delta = {locale: dict(zip(KEYS, values)) for locale, values in ROWS.items()}
    assert set(FORMAT_ROWS) == set(ROWS)
    assert all(len(values) == len(FORMAT_KEYS) for values in FORMAT_ROWS.values())
    for locale, values in FORMAT_ROWS.items():
        delta[locale].update(zip(FORMAT_KEYS, values))
    assert set(USAGE_ROWS) == set(ROWS)
    assert all(len(values) == len(USAGE_KEYS) for values in USAGE_ROWS.values())
    for locale, values in USAGE_ROWS.items():
        delta[locale].update(zip(USAGE_KEYS, values))
    assert set(LAYER_LABEL) == set(ROWS)
    for locale, value in LAYER_LABEL.items():
        delta[locale]['Afficher les calques'] = value
    (HERE / 'drawing-delta-0835.json').write_text(json.dumps(delta, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(len(delta['fr-FR']), 'keys in', len(delta), 'languages')


if __name__ == '__main__':
    main()
