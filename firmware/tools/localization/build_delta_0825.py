"""Reviewed .25 UI translations; no fallback or automatic padding."""
from pathlib import Path
import json
HERE=Path(__file__).resolve().parent
KEYS=['Ouvrir le dossier courant dans le terminal','Exécuter le script','Impossible de lancer le script.',"Ouvrir l'application",'Dossier parent · Retour arrière','Correction (ppm)','Désaccentuation FM','Réglages de réception invalides','Coffre de mots de passe','Environnement indisponible.','Variable protégée inaccessible.','Commande masquée','Secret identifiant Git','Secret jeton Git','Choisir les deux secrets Git, ou aucun.','Aucun','Ctrl+Maj+N','Ctrl+Maj+H']
ROWS={
'en-US':'''Open current folder in terminal
Run script
Cannot run the script.
Open application
Parent folder · Backspace
Correction (ppm)
FM de-emphasis
Invalid reception settings
Password vault
Environment unavailable.
Protected variable unavailable.
Hidden command
Git username secret
Git token secret
Choose both Git secrets, or neither.
None
Ctrl+Shift+N
Ctrl+Shift+H''',
'de-DE':'''Aktuellen Ordner im Terminal öffnen
Skript ausführen
Das Skript kann nicht gestartet werden.
Anwendung öffnen
Übergeordneter Ordner · Rücktaste
Korrektur (ppm)
FM-Deemphasis
Ungültige Empfangseinstellungen
Passworttresor
Umgebung nicht verfügbar.
Geschützte Variable nicht verfügbar.
Verborgener Befehl
Geheimer Git-Benutzername
Geheimes Git-Token
Beide Git-Geheimnisse auswählen oder keines.
Keines
Strg+Umschalt+N
Strg+Umschalt+H''',
'es-ES':'''Abrir la carpeta actual en el terminal
Ejecutar el script
No se puede ejecutar el script.
Abrir la aplicación
Carpeta superior · Retroceso
Corrección (ppm)
Desénfasis FM
Ajustes de recepción no válidos
Caja fuerte de contraseñas
Entorno no disponible.
Variable protegida no disponible.
Comando oculto
Secreto de usuario Git
Secreto de token Git
Elige ambos secretos Git o ninguno.
Ninguno
Ctrl+Mayús+N
Ctrl+Mayús+H''',
'it-IT':'''Apri la cartella corrente nel terminale
Esegui lo script
Impossibile eseguire lo script.
Apri l'applicazione
Cartella superiore · Backspace
Correzione (ppm)
Deenfasi FM
Impostazioni di ricezione non valide
Cassaforte delle password
Ambiente non disponibile.
Variabile protetta non accessibile.
Comando nascosto
Segreto del nome utente Git
Segreto del token Git
Scegli entrambi i segreti Git o nessuno.
Nessuno
Ctrl+Maiusc+N
Ctrl+Maiusc+H''',
'nl-NL':'''Huidige map in terminal openen
Script uitvoeren
Het script kan niet worden uitgevoerd.
Toepassing openen
Bovenliggende map · Backspace
Correctie (ppm)
FM-deëmfase
Ongeldige ontvangstinstellingen
Wachtwoordkluis
Omgeving niet beschikbaar.
Beveiligde variabele niet beschikbaar.
Verborgen opdracht
Geheime Git-gebruikersnaam
Geheim Git-token
Kies beide Git-geheimen of geen van beide.
Geen
Ctrl+Shift+N
Ctrl+Shift+H''',
'pt-BR':'''Abrir a pasta atual no terminal
Executar o script
Não foi possível executar o script.
Abrir o aplicativo
Pasta superior · Backspace
Correção (ppm)
Deênfase FM
Configurações de recepção inválidas
Cofre de senhas
Ambiente indisponível.
Variável protegida inacessível.
Comando oculto
Segredo do usuário Git
Segredo do token Git
Escolha ambos os segredos Git ou nenhum.
Nenhum
Ctrl+Shift+N
Ctrl+Shift+H''',
'pt-PT':'''Abrir a pasta atual no terminal
Executar o script
Não foi possível executar o script.
Abrir a aplicação
Pasta superior · Backspace
Correção (ppm)
Desênfase FM
Definições de receção inválidas
Cofre de palavras-passe
Ambiente indisponível.
Variável protegida inacessível.
Comando oculto
Segredo do utilizador Git
Segredo do token Git
Escolha ambos os segredos Git ou nenhum.
Nenhum
Ctrl+Shift+N
Ctrl+Shift+H''',
'id-ID':'''Buka folder saat ini di terminal
Jalankan skrip
Tidak dapat menjalankan skrip.
Buka aplikasi
Folder induk · Backspace
Koreksi (ppm)
Deemfasis FM
Pengaturan penerimaan tidak valid
Brankas kata sandi
Lingkungan tidak tersedia.
Variabel terlindungi tidak dapat diakses.
Perintah disembunyikan
Rahasia nama pengguna Git
Rahasia token Git
Pilih kedua rahasia Git atau tidak keduanya.
Tidak ada
Ctrl+Shift+N
Ctrl+Shift+H''',
'ru-RU':'''Открыть текущую папку в терминале
Выполнить скрипт
Не удалось выполнить скрипт.
Открыть приложение
Родительская папка · Backspace
Коррекция (ppm)
Деэмфазис FM
Неверные параметры приёма
Хранилище паролей
Окружение недоступно.
Защищённая переменная недоступна.
Скрытая команда
Секрет имени пользователя Git
Секрет токена Git
Выберите оба секрета Git или ни одного.
Нет
Ctrl+Shift+N
Ctrl+Shift+H''',
'zh-CN':'''在终端中打开当前文件夹
运行脚本
无法运行脚本。
打开应用
上级文件夹 · Backspace
校正（ppm）
FM 去加重
接收设置无效
密码保险库
环境不可用。
无法访问受保护的变量。
隐藏的命令
Git 用户名机密
Git 令牌机密
请选择两个 Git 机密，或都不选。
无
Ctrl+Shift+N
Ctrl+Shift+H''',
'ja-JP':'''現在のフォルダーを端末で開く
スクリプトを実行
スクリプトを実行できません。
アプリを開く
親フォルダー · Backspace
補正（ppm）
FM デエンファシス
受信設定が無効です
パスワード保管庫
環境を利用できません。
保護された変数にアクセスできません。
非表示のコマンド
Git ユーザー名のシークレット
Git トークンのシークレット
Git シークレットを両方選ぶか、両方とも未選択にしてください。
なし
Ctrl+Shift+N
Ctrl+Shift+H''',
'ko-KR':'''현재 폴더를 터미널에서 열기
스크립트 실행
스크립트를 실행할 수 없습니다.
앱 열기
상위 폴더 · Backspace
보정(ppm)
FM 디엠퍼시스
잘못된 수신 설정
비밀번호 보관함
환경을 사용할 수 없습니다.
보호된 변수에 접근할 수 없습니다.
숨겨진 명령
Git 사용자 이름 비밀
Git 토큰 비밀
Git 비밀 두 개를 모두 선택하거나 모두 선택하지 마세요.
없음
Ctrl+Shift+N
Ctrl+Shift+H''',
'ar':'''فتح المجلد الحالي في الطرفية
تشغيل السكربت
تعذر تشغيل السكربت.
فتح التطبيق
المجلد الأب · Backspace
التصحيح (ppm)
إزالة التوكيد في FM
إعدادات استقبال غير صالحة
خزنة كلمات المرور
البيئة غير متاحة.
المتغير المحمي غير متاح.
أمر مخفي
سر اسم مستخدم Git
سر رمز Git
اختر سري Git معًا أو لا تختر أيًا منهما.
لا شيء
Ctrl+Shift+N
Ctrl+Shift+H'''}
delta={'fr-FR':dict(zip(KEYS,KEYS))}
for locale,text in ROWS.items():
    values=text.splitlines();assert len(values)==len(KEYS),(locale,len(values),len(KEYS))
    delta[locale]=dict(zip(KEYS,values))
(HERE/'delta-ui-0825.json').write_text(json.dumps(delta,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('Prepared',len(KEYS),'UI keys in',len(delta),'languages')
