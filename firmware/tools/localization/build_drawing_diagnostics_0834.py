from pathlib import Path
import json
HERE=Path(__file__).resolve().parent
KEYS='''Lecture JPEG impossible.
Import JPEG annule; image conservee.
Memoire insuffisante pour le decodeur JPEG.
JPEG trop volumineux (16 Mio maximum).
JPEG non pris en charge (utiliser un JPEG standard).
JPEG incomplet ou invalide; image conservee.
Dimensions JPEG invalides (8192 maximum).
Memoire insuffisante pour le pinceau; image conservee.
Memoire insuffisante pour la deformation; image conservee.
Geste interrompu; recommencer le trait.
Image JPEG
Image BMP'''.splitlines()
ROWS={}
ROWS['fr-FR']='''Lecture JPEG impossible.
Import JPEG annulé ; image conservée.
Mémoire insuffisante pour le décodeur JPEG.
JPEG trop volumineux (16 Mio maximum).
JPEG non pris en charge (utiliser un JPEG standard).
JPEG incomplet ou invalide ; image conservée.
Dimensions JPEG invalides (8192 maximum).
Mémoire insuffisante pour le pinceau ; image conservée.
Mémoire insuffisante pour la déformation ; image conservée.
Geste interrompu ; recommencer le trait.
Image JPEG
Image BMP'''.splitlines()
ROWS['en-US']='''Could not read JPEG.
JPEG import cancelled; image preserved.
Not enough memory for the JPEG decoder.
JPEG too large (16 MiB maximum).
Unsupported JPEG (use a baseline JPEG).
Incomplete or invalid JPEG; image preserved.
Invalid JPEG dimensions (8192 maximum).
Not enough memory for the brush; image preserved.
Not enough memory for deformation; image preserved.
Gesture interrupted; start the stroke again.
JPEG image
BMP image'''.splitlines()
ROWS['de-DE']='''JPEG konnte nicht gelesen werden.
JPEG-Import abgebrochen; Bild erhalten.
Nicht genug Speicher für den JPEG-Decoder.
JPEG zu groß (höchstens 16 MiB).
JPEG nicht unterstützt (Standard-JPEG verwenden).
JPEG unvollständig oder ungültig; Bild erhalten.
Ungültige JPEG-Abmessungen (höchstens 8192).
Nicht genug Speicher für den Pinsel; Bild erhalten.
Nicht genug Speicher für die Verformung; Bild erhalten.
Geste unterbrochen; Strich neu beginnen.
JPEG-Bild
BMP-Bild'''.splitlines()
ROWS['es-ES']='''No se pudo leer el JPEG.
Importación JPEG cancelada; imagen conservada.
Memoria insuficiente para el decodificador JPEG.
JPEG demasiado grande (máximo 16 MiB).
JPEG no compatible (usar un JPEG estándar).
JPEG incompleto o no válido; imagen conservada.
Dimensiones JPEG no válidas (máximo 8192).
Memoria insuficiente para el pincel; imagen conservada.
Memoria insuficiente para la deformación; imagen conservada.
Gesto interrumpido; reiniciar el trazo.
Imagen JPEG
Imagen BMP'''.splitlines()
ROWS['it-IT']='''Impossibile leggere il JPEG.
Importazione JPEG annullata; immagine conservata.
Memoria insufficiente per il decodificatore JPEG.
JPEG troppo grande (massimo 16 MiB).
JPEG non supportato (usare un JPEG standard).
JPEG incompleto o non valido; immagine conservata.
Dimensioni JPEG non valide (massimo 8192).
Memoria insufficiente per il pennello; immagine conservata.
Memoria insufficiente per la deformazione; immagine conservata.
Gesto interrotto; ricominciare il tratto.
Immagine JPEG
Immagine BMP'''.splitlines()
ROWS['nl-NL']='''Kan JPEG niet lezen.
JPEG-import geannuleerd; afbeelding behouden.
Onvoldoende geheugen voor de JPEG-decoder.
JPEG te groot (maximaal 16 MiB).
JPEG niet ondersteund (gebruik een standaard-JPEG).
JPEG onvolledig of ongeldig; afbeelding behouden.
Ongeldige JPEG-afmetingen (maximaal 8192).
Onvoldoende geheugen voor het penseel; afbeelding behouden.
Onvoldoende geheugen voor vervorming; afbeelding behouden.
Gebaar onderbroken; begin de streek opnieuw.
JPEG-afbeelding
BMP-afbeelding'''.splitlines()
ROWS['pt-BR']='''Não foi possível ler o JPEG.
Importação JPEG cancelada; imagem preservada.
Memória insuficiente para o decodificador JPEG.
JPEG muito grande (máximo de 16 MiB).
JPEG não compatível (use um JPEG padrão).
JPEG incompleto ou inválido; imagem preservada.
Dimensões JPEG inválidas (máximo de 8192).
Memória insuficiente para o pincel; imagem preservada.
Memória insuficiente para a deformação; imagem preservada.
Gesto interrompido; reinicie o traço.
Imagem JPEG
Imagem BMP'''.splitlines()
ROWS['pt-PT']='''Não foi possível ler o JPEG.
Importação JPEG cancelada; imagem preservada.
Memória insuficiente para o descodificador JPEG.
JPEG demasiado grande (máximo de 16 MiB).
JPEG não suportado (utilize um JPEG padrão).
JPEG incompleto ou inválido; imagem preservada.
Dimensões JPEG inválidas (máximo de 8192).
Memória insuficiente para o pincel; imagem preservada.
Memória insuficiente para a deformação; imagem preservada.
Gesto interrompido; reinicie o traço.
Imagem JPEG
Imagem BMP'''.splitlines()
ROWS['id-ID']='''Tidak dapat membaca JPEG.
Impor JPEG dibatalkan; gambar dipertahankan.
Memori tidak cukup untuk dekoder JPEG.
JPEG terlalu besar (maksimum 16 MiB).
JPEG tidak didukung (gunakan JPEG standar).
JPEG tidak lengkap atau tidak valid; gambar dipertahankan.
Dimensi JPEG tidak valid (maksimum 8192).
Memori tidak cukup untuk kuas; gambar dipertahankan.
Memori tidak cukup untuk deformasi; gambar dipertahankan.
Gerakan terputus; mulai ulang goresan.
Gambar JPEG
Gambar BMP'''.splitlines()
ROWS['ru-RU']='''Не удалось прочитать JPEG.
Импорт JPEG отменён; изображение сохранено.
Недостаточно памяти для декодера JPEG.
JPEG слишком велик (не более 16 МиБ).
JPEG не поддерживается (нужен стандартный JPEG).
JPEG неполный или неверный; изображение сохранено.
Неверные размеры JPEG (не более 8192).
Недостаточно памяти для кисти; изображение сохранено.
Недостаточно памяти для деформации; изображение сохранено.
Жест прерван; начните штрих заново.
JPEG
BMP'''.splitlines()
ROWS['ar']='''تعذرت قراءة JPEG.
أُلغي استيراد JPEG؛ الصورة محفوظة.
الذاكرة غير كافية لفك ترميز JPEG.
ملف JPEG كبير جداً (الحد الأقصى 16 MiB).
JPEG غير مدعوم (استخدم JPEG قياسياً).
JPEG غير مكتمل أو غير صالح؛ الصورة محفوظة.
أبعاد JPEG غير صالحة (الحد الأقصى 8192).
الذاكرة غير كافية للفرشاة؛ الصورة محفوظة.
الذاكرة غير كافية للتشويه؛ الصورة محفوظة.
توقفت الحركة؛ ابدأ الخط من جديد.
صورة JPEG
صورة BMP'''.splitlines()
ROWS['zh-CN']='''无法读取 JPEG。
已取消 JPEG 导入；图像保持不变。
内存不足，无法运行 JPEG 解码器。
JPEG 过大（最大 16 MiB）。
不支持此 JPEG（请使用标准 JPEG）。
JPEG 不完整或无效；图像保持不变。
JPEG 尺寸无效（最大 8192）。
画笔内存不足；图像保持不变。
变形内存不足；图像保持不变。
操作已中断；请重新开始笔画。
JPEG 图像
BMP 图像'''.splitlines()
ROWS['ja-JP']='''JPEGを読み込めません。
JPEGの読み込みを中止しました。画像は保持されています。
JPEGデコーダー用のメモリーが不足しています。
JPEGが大きすぎます（最大16 MiB）。
非対応のJPEGです（標準JPEGを使用してください）。
JPEGが不完全または無効です。画像は保持されています。
JPEGのサイズが無効です（最大8192）。
ブラシ用のメモリーが不足しています。画像は保持されています。
変形用のメモリーが不足しています。画像は保持されています。
操作が中断されました。線を引き直してください。
JPEG画像
BMP画像'''.splitlines()
ROWS['ko-KR']='''JPEG를 읽을 수 없습니다.
JPEG 가져오기를 취소했습니다. 이미지는 유지됩니다.
JPEG 디코더용 메모리가 부족합니다.
JPEG가 너무 큽니다(최대 16 MiB).
지원하지 않는 JPEG입니다(표준 JPEG를 사용하세요).
JPEG가 불완전하거나 올바르지 않습니다. 이미지는 유지됩니다.
JPEG 크기가 올바르지 않습니다(최대 8192).
브러시용 메모리가 부족합니다. 이미지는 유지됩니다.
변형용 메모리가 부족합니다. 이미지는 유지됩니다.
동작이 중단되었습니다. 획을 다시 그려 주세요.
JPEG 이미지
BMP 이미지'''.splitlines()
for locale,values in ROWS.items():
 assert len(values)==len(KEYS),(locale,len(values),len(KEYS))
 assert len(values[-1].encode())<=23 and len(values[-2].encode())<=23,(locale,'layer name')
DELTA={locale:dict(zip(KEYS,values)) for locale,values in ROWS.items()}
if __name__=='__main__':
 (HERE/'drawing-diagnostics-delta-0834.json').write_text(json.dumps(DELTA,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print('Diagnostics staged',len(KEYS),'keys',len(DELTA),'languages')
