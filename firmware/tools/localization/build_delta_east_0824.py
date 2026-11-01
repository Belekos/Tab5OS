"""Reviewed translations aligned with the immutable 0.8.24 delta reference."""
from pathlib import Path
import json

HERE = Path(__file__).resolve().parent
TRANSLATIONS = {
'ru-RU': '''Сортировать по
Имя: от А до Я
Имя: от Я до А
Дата: сначала старые
Дата: сначала новые
Размер: по возрастанию
Размер: по убыванию
Последний вид не сохранён
Выберите «Открыть», чтобы просмотреть изображение.
Загрузка.
Недостаточно памяти — используйте поиск
Настройки карты
Запоминать последний вид
Положение и масштаб сохраняются при выходе из Карт. Если отключено, открывается начальный вид.
Обнаружения ожидают обработки: сканирование приостановлено
Устройство отклонило частоту или вход
Устройство отклонило частоту дискретизации
Сканирование
Сканирование завершено
Спектр активен: аудиовыход занят
Сигнал обнаружен: прослушивание при паузе сканирования
Недопустимая частота или режим
Поиск тюнера
Остановка
Недопустимый или слишком широкий диапазон сканирования
Сканирование приостановлено: приём на фиксированной частоте
Произвольный
Длинные волны
Средние волны
Короткие волны
FM 64–108 МГц
FM 87,5–108 МГц
Авиационный УКВ
УКВ 144–148 МГц
Морской УКВ
УВЧ 430–440 МГц
УВЧ 446 МГц
УВЧ 863–870 МГц
Недопустимая частота
Ничего не обнаружено
Обнаружения
Недопустимый файл обнаружений
Обнаружения сохранены в /System/SDR/detections.csv
Недопустимый шаг частоты
Для прослушивания выберите AM, WFM или NFM
Недопустимые оси: минимум должен быть меньше максимума
Список заполнен: удалите обнаружения
Частота
Единица
Шаг (кГц)
Демодуляция
Спектр
Принимать / применить
Сканирование
Ширина X
Смещение X (кГц)
Мин. Y (dBFS)
Макс. Y (dBFS)
Применить оси
Запомнить
Фиксированный приём
Предлагаемый диапазон
Тюнер
Прямой I
Прямой Q
Диапазоны ориентировочные и изменяемые. Допустимые частоты определяет устройство. Для прямого I/Q нужен подходящий аппаратный вход.
Начало (кГц)
Конец (кГц)
Длительность шага (мс)
Порог (dBFS)
Приостанавливать сканирование при обнаружении сигнала
Сканировать диапазон
Один проход, не более 20 000 шагов. Порог обнаруживает сигнал, но не определяет станцию. Шаг задаётся выше.
Принимать выбранное
Очистить список
Сохранить список
Не более 128 обнаружений. Выбор сам по себе не меняет приём. Сохранение также фиксирует удаления.
Отчёты приняты / отклонены
Ошибки USB / перезапуски
Фильтр медиа
Выбор сохранён
Недостаточно памяти для списка медиа''',
'id-ID': '''Urutkan menurut
Nama: A ke Z
Nama: Z ke A
Tanggal: terlama dahulu
Tanggal: terbaru dahulu
Ukuran: terkecil dahulu
Ukuran: terbesar dahulu
Tampilan terakhir tidak tersimpan
Pilih Buka untuk menampilkan gambar ini.
Memuat.
Memori tidak cukup — gunakan pencarian
Pengaturan peta
Ingat tampilan terakhir
Posisi dan zoom disimpan saat keluar dari Peta. Jika nonaktif, buka tampilan awal.
Deteksi menunggu: pemindaian dijeda
Frekuensi atau masukan ditolak perangkat
Laju sampel ditolak perangkat
Pemindaian berlangsung
Pemindaian selesai
Spektrum aktif: keluaran audio sedang digunakan
Sinyal terdeteksi: mendengarkan saat pemindaian dijeda
Frekuensi atau mode tidak valid
Mencari tuner
Menghentikan
Pemindaian tidak valid atau terlalu luas
Pemindaian dijeda: penerimaan tetap
Bebas
Gelombang panjang
Gelombang menengah
Gelombang pendek
FM 64–108 MHz
FM 87,5–108 MHz
VHF penerbangan
VHF 144–148 MHz
VHF maritim
UHF 430–440 MHz
UHF 446 MHz
UHF 863–870 MHz
Frekuensi tidak valid
Tidak ada deteksi
Deteksi
Berkas deteksi tidak valid
Deteksi disimpan di /System/SDR/detections.csv
Langkah frekuensi tidak valid
Pilih AM, WFM, atau NFM untuk mendengarkan
Sumbu tidak valid: minimum harus lebih kecil dari maksimum
Daftar penuh: hapus beberapa deteksi
Frekuensi
Satuan
Langkah (kHz)
Demodulasi
Spektrum
Terima / terapkan
Pemindaian
Lebar X
Pergeseran X (kHz)
Y minimum (dBFS)
Y maksimum (dBFS)
Terapkan sumbu
Ingat
Penerimaan tetap
Rentang yang disarankan
Tuner
I langsung
Q langsung
Rentang hanya panduan dan dapat diubah. Perangkat menentukan frekuensi yang diterima. I/Q langsung memerlukan masukan perangkat keras yang sesuai.
Awal (kHz)
Akhir (kHz)
Durasi per langkah (ms)
Ambang (dBFS)
Jeda pemindaian saat ada sinyal
Pindai rentang
Satu putaran, maksimal 20.000 langkah. Ambang mendeteksi sinyal tanpa mengenali stasiun. Langkah diatur di atas.
Terima pilihan
Kosongkan daftar
Simpan daftar
Maksimal 128 deteksi. Memilih saja tidak mengubah penerimaan. Menyimpan juga mempertahankan penghapusan.
Laporan diterima / ditolak
Kesalahan USB / pemulihan
Filter media
Pilihan tersimpan
Memori tidak cukup untuk daftar media''',
'zh-CN': '''排序方式
名称：A 到 Z
名称：Z 到 A
日期：最早优先
日期：最新优先
大小：从小到大
大小：从大到小
未保存上次视图
选择“打开”以显示此图片。
正在加载。
内存不足，请使用搜索
地图设置
记住上次视图
退出地图时保存位置和缩放级别。关闭后打开初始视图。
检测结果待处理：扫描已暂停
设备不接受该频率或输入
设备不接受该采样率
正在扫描
扫描完成
频谱已启用：音频输出被占用
检测到信号：扫描暂停，正在收听
频率或模式无效
正在查找调谐器
正在停止
扫描范围无效或过大
扫描已暂停：定频收听
自定义
长波
中波
短波
FM 64–108 MHz
FM 87.5–108 MHz
航空 VHF
VHF 144–148 MHz
海事 VHF
UHF 430–440 MHz
UHF 446 MHz
UHF 863–870 MHz
频率无效
未检测到信号
检测结果
检测结果文件无效
检测结果已保存到 /System/SDR/detections.csv
频率步长无效
请选择 AM、WFM 或 NFM 进行收听
坐标轴无效：最小值必须小于最大值
列表已满，请删除部分检测结果
频率
单位
步长 (kHz)
解调
频谱
接收 / 应用
扫描
X 轴宽度
X 轴偏移 (kHz)
Y 轴最小值 (dBFS)
Y 轴最大值 (dBFS)
应用坐标轴
记住
定频收听
建议范围
调谐器
I 路直采
Q 路直采
范围仅供参考，可自行修改。可用频率由设备决定。I/Q 直采需要相应的硬件输入。
起始 (kHz)
结束 (kHz)
每步时长 (ms)
阈值 (dBFS)
检测到信号时暂停扫描
扫描此范围
单次扫描，最多 20,000 步。阈值用于检测信号，不识别电台。步长在上方设置。
接收所选项
清空列表
保存列表
最多 128 条检测结果。仅选择条目不会改变接收状态。保存时也会保留删除操作。
已接收 / 拒绝的报告
USB 错误 / 恢复
筛选媒体
已保存选择
内存不足，无法加载媒体列表''',
'ja-JP': '''並べ替え
名前：A から Z
名前：Z から A
日付：古い順
日付：新しい順
サイズ：小さい順
サイズ：大きい順
前回の表示を保存できませんでした
この画像を表示するには「開く」を選んでください。
読み込み中。
メモリ不足です。検索を使用してください
地図の設定
前回の表示を記憶
地図を閉じると位置とズームを保存します。無効の場合は初期表示を開きます。
検出結果の処理待ち：スキャンを一時停止
デバイスが周波数または入力を受け付けません
デバイスがサンプルレートを受け付けません
スキャン中
スキャン完了
スペクトル表示中：音声出力は使用中です
信号を検出：スキャンを一時停止して受信中
周波数またはモードが無効です
チューナーを検索中
停止中
スキャン範囲が無効または広すぎます
スキャンを一時停止：固定周波数で受信
自由設定
長波
中波
短波
FM 64–108 MHz
FM 87.5–108 MHz
航空 VHF
VHF 144–148 MHz
船舶 VHF
UHF 430–440 MHz
UHF 446 MHz
UHF 863–870 MHz
周波数が無効です
検出なし
検出結果
検出結果ファイルが無効です
検出結果を /System/SDR/detections.csv に保存しました
周波数ステップが無効です
聴取には AM、WFM、NFM のいずれかを選択してください
軸の設定が無効です。最小値を最大値より小さくしてください
リストがいっぱいです。検出結果を削除してください
周波数
単位
ステップ (kHz)
復調
スペクトル
受信 / 適用
スキャン
X 軸幅
X 軸オフセット (kHz)
Y 最小値 (dBFS)
Y 最大値 (dBFS)
軸の設定を適用
記憶
固定周波数で受信
推奨範囲
チューナー
I 直接入力
Q 直接入力
範囲は目安であり変更できます。受信可能な周波数はデバイスによって異なります。I/Q 直接入力には対応するハードウェア入力が必要です。
開始 (kHz)
終了 (kHz)
各ステップの時間 (ms)
しきい値 (dBFS)
信号を検出したらスキャンを一時停止
範囲をスキャン
1 回の走査で最大 20,000 ステップ。しきい値は信号を検出しますが、放送局は識別しません。ステップは上で設定します。
選択項目を受信
リストを消去
リストを保存
最大 128 件の検出結果。選択だけでは受信は変わりません。保存すると削除内容も保持されます。
受信 / 拒否レポート
USB エラー / 復旧
メディアを絞り込む
選択を記憶しました
メディアリスト用のメモリが不足しています''',
'ko-KR': '''정렬 기준
이름: A부터 Z까지
이름: Z부터 A까지
날짜: 오래된 순
날짜: 최신순
크기: 작은 순
크기: 큰 순
마지막 화면이 저장되지 않음
이 이미지를 보려면 열기를 선택하세요.
불러오는 중.
메모리 부족 — 검색을 사용하세요
지도 설정
마지막 화면 기억
지도를 나갈 때 위치와 확대 수준을 저장합니다. 끄면 초기 화면을 엽니다.
감지 결과 처리 대기: 스캔 일시 중지
장치가 주파수 또는 입력을 거부함
장치가 샘플 레이트를 거부함
스캔 중
스캔 완료
스펙트럼 활성: 오디오 출력 사용 중
신호 감지: 스캔을 일시 중지하고 수신 중
잘못된 주파수 또는 모드
튜너 검색 중
중지 중
스캔 범위가 잘못되었거나 너무 넓음
스캔 일시 중지: 고정 주파수 수신
자유 설정
장파
중파
단파
FM 64–108 MHz
FM 87.5–108 MHz
항공 VHF
VHF 144–148 MHz
해상 VHF
UHF 430–440 MHz
UHF 446 MHz
UHF 863–870 MHz
잘못된 주파수
감지 결과 없음
감지 결과
잘못된 감지 결과 파일
감지 결과가 /System/SDR/detections.csv에 저장됨
잘못된 주파수 간격
청취하려면 AM, WFM 또는 NFM을 선택하세요
잘못된 축 설정: 최솟값은 최댓값보다 작아야 함
목록이 가득 참: 감지 결과를 삭제하세요
주파수
단위
간격 (kHz)
복조
스펙트럼
수신 / 적용
스캔
X축 폭
X축 오프셋 (kHz)
Y 최솟값 (dBFS)
Y 최댓값 (dBFS)
축 설정 적용
기억
고정 주파수 수신
권장 범위
튜너
I 직접 입력
Q 직접 입력
범위는 참고용이며 수정할 수 있습니다. 지원 주파수는 장치가 결정합니다. I/Q 직접 입력에는 적합한 하드웨어 입력이 필요합니다.
시작 (kHz)
종료 (kHz)
단계별 시간 (ms)
임계값 (dBFS)
신호를 감지하면 스캔 일시 중지
범위 스캔
한 번에 최대 20,000단계. 임계값은 신호를 감지하지만 방송국을 식별하지 않습니다. 간격은 위에서 설정합니다.
선택 항목 수신
목록 비우기
목록 저장
감지 결과는 최대 128개입니다. 선택만으로 수신이 바뀌지 않습니다. 저장하면 삭제한 항목도 반영됩니다.
수신 / 거부 보고서
USB 오류 / 복구
미디어 필터
선택 사항 저장됨
미디어 목록을 위한 메모리 부족''',
'ar': '''الترتيب حسب
الاسم: من A إلى Z
الاسم: من Z إلى A
التاريخ: الأقدم أولاً
التاريخ: الأحدث أولاً
الحجم: تصاعدياً
الحجم: تنازلياً
لم يُحفظ العرض الأخير
اختر فتح لعرض هذه الصورة.
جارٍ التحميل.
الذاكرة غير كافية — استخدم البحث
إعدادات الخريطة
تذكر العرض الأخير
يُحفظ الموضع والتكبير عند مغادرة الخرائط. عند التعطيل يُفتح العرض الأولي.
نتائج الكشف في الانتظار: المسح متوقف مؤقتاً
رفض الجهاز التردد أو المدخل
رفض الجهاز معدل أخذ العينات
جارٍ المسح
اكتمل المسح
الطيف نشط: خرج الصوت مشغول
اكتُشفت إشارة: استماع أثناء إيقاف المسح مؤقتاً
التردد أو الوضع غير صالح
جارٍ البحث عن الموالف
جارٍ الإيقاف
نطاق المسح غير صالح أو واسع جداً
المسح متوقف مؤقتاً: استقبال على تردد ثابت
حر
الموجات الطويلة
الموجات المتوسطة
الموجات القصيرة
FM 64–108 MHz
FM 87.5–108 MHz
VHF للطيران
VHF 144–148 MHz
VHF بحري
UHF 430–440 MHz
UHF 446 MHz
UHF 863–870 MHz
تردد غير صالح
لا توجد نتائج كشف
نتائج الكشف
ملف نتائج الكشف غير صالح
حُفظت نتائج الكشف في /System/SDR/detections.csv
خطوة التردد غير صالحة
اختر AM أو WFM أو NFM للاستماع
المحاور غير صالحة: يجب أن يكون الحد الأدنى أقل من الأقصى
القائمة ممتلئة: احذف بعض نتائج الكشف
التردد
الوحدة
الخطوة (kHz)
إزالة التضمين
الطيف
استقبال / تطبيق
المسح
عرض X
إزاحة X (kHz)
الحد الأدنى لـ Y (dBFS)
الحد الأقصى لـ Y (dBFS)
تطبيق المحاور
تذكر
استقبال ثابت
النطاق المقترح
الموالف
I مباشر
Q مباشر
النطاقات إرشادية وقابلة للتعديل. يحدد الجهاز الترددات المقبولة. يتطلب I/Q المباشر مدخلاً مادياً مناسباً.
البداية (kHz)
النهاية (kHz)
المدة لكل خطوة (ms)
العتبة (dBFS)
إيقاف المسح مؤقتاً عند رصد إشارة
مسح النطاق
مرور واحد، بحد أقصى 20,000 خطوة. ترصد العتبة إشارة دون تحديد المحطة. تُضبط الخطوة أعلاه.
استقبال المحدد
مسح القائمة
حفظ القائمة
128 نتيجة كشف كحد أقصى. الاختيار وحده لا يغير الاستقبال. يحفظ الأمر أيضاً عمليات الحذف.
التقارير المستلمة / المرفوضة
أخطاء USB / الاستئناف
تصفية الوسائط
حُفظ الاختيار
الذاكرة غير كافية لقائمة الوسائط'''
}

def main():
    reference = json.loads((HERE/'reference-delta-0824.json').read_text(encoding='utf-8'))
    delta = {}
    for locale, text in TRANSLATIONS.items():
        values = text.splitlines()
        assert len(values) == len(reference), (locale, len(values), len(reference))
        assert all(v.strip() and '\ufffd' not in v for v in values)
        delta[locale] = dict(zip(reference, values))
    (HERE/'delta-east-0824.json').write_text(json.dumps(delta,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    sketch = HERE.parents[1]/'Tab5OS_Arduino_v0.8.24_Ecriture'
    for locale, translations in delta.items():
        path = sketch/'sdcard/System/languages'/f'{locale}.json'
        pack = json.loads(path.read_text(encoding='utf-8'))
        pack['translations'].update(translations)
        path.write_text(json.dumps(pack,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(f'Integrated {len(reference)} translations into each of {len(delta)} language packs.')

if __name__ == '__main__':
    main()
