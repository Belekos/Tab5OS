"""Preserve old Git help translations and append the reviewed secret workflow."""
from pathlib import Path
import json,re,zipfile
HERE=Path(__file__).resolve().parent;RELEASE=HERE.parents[1]
SKETCH=next(RELEASE.glob('Tab5OS_Arduino_*_Ecriture'))
ROWS={
'fr-FR':[
"Les références usernameSecret et tokenSecret lisent le coffre déverrouillé lors de l’authentification. clone accepte --username-secret NOM --token-secret NOM. Le fichier d’identification reste compatible. config --unset CLE retire une référence.",
"Partage une variable publique en mémoire jusqu'au redémarrage ; --secret lie un nom à une entrée chiffrée du coffre, résolue seulement lorsque le coffre est déverrouillé.",
'Liste uniquement les noms des variables. Pour lire volontairement une valeur, utilisez echo "$NOM".'],
'en-US':[
'The usernameSecret and tokenSecret references read the unlocked vault during authentication. clone accepts --username-secret NAME --token-secret NAME. Credential files remain supported. config --unset KEY removes a reference.',
'Shares a public variable in memory until restart; --secret binds a name to an encrypted vault entry, resolved only while the vault is unlocked.',
'Lists variable names only. To intentionally read a value, use echo "$NOM".'],
'de-DE':[
'Die Verweise usernameSecret und tokenSecret lesen bei der Anmeldung aus dem entsperrten Tresor. clone unterstützt --username-secret NAME --token-secret NAME. Anmeldedateien bleiben kompatibel. config --unset SCHLÜSSEL entfernt einen Verweis.',
'Teilt eine öffentliche Variable im Arbeitsspeicher bis zum Neustart; --secret bindet einen Namen an einen verschlüsselten Tresoreintrag, der nur bei entsperrtem Tresor aufgelöst wird.',
'Listet nur Variablennamen auf. Um einen Wert ausdrücklich zu lesen, verwenden Sie echo "$NOM".'],
'es-ES':[
'Las referencias usernameSecret y tokenSecret leen la caja fuerte desbloqueada durante la autenticación. clone admite --username-secret NOMBRE --token-secret NOMBRE. Los archivos de credenciales siguen siendo compatibles. config --unset CLAVE elimina una referencia.',
'Comparte una variable pública en memoria hasta el reinicio; --secret vincula un nombre a una entrada cifrada de la caja fuerte, que solo se resuelve cuando está desbloqueada.',
'Solo enumera los nombres de las variables. Para leer expresamente un valor, usa echo "$NOM".'],
'it-IT':[
'I riferimenti usernameSecret e tokenSecret leggono la cassaforte sbloccata durante l’autenticazione. clone accetta --username-secret NOME --token-secret NOME. I file delle credenziali restano compatibili. config --unset CHIAVE rimuove un riferimento.',
'Condivide una variabile pubblica in memoria fino al riavvio; --secret associa un nome a una voce cifrata della cassaforte, risolta solo quando la cassaforte è sbloccata.',
'Elenca solo i nomi delle variabili. Per leggere esplicitamente un valore, usa echo "$NOM".'],
'nl-NL':[
'De verwijzingen usernameSecret en tokenSecret lezen de ontgrendelde kluis tijdens authenticatie. clone accepteert --username-secret NAAM --token-secret NAAM. Bestanden met aanmeldgegevens blijven ondersteund. config --unset SLEUTEL verwijdert een verwijzing.',
'Deelt een openbare variabele in het geheugen tot de herstart; --secret koppelt een naam aan een versleuteld kluisitem, dat alleen wordt uitgelezen wanneer de kluis ontgrendeld is.',
'Toont alleen variabelenamen. Gebruik echo "$NOM" om bewust een waarde te lezen.'],
'pt-BR':[
'As referências usernameSecret e tokenSecret leem o cofre desbloqueado durante a autenticação. clone aceita --username-secret NOME --token-secret NOME. Os arquivos de credenciais continuam compatíveis. config --unset CHAVE remove uma referência.',
'Compartilha uma variável pública na memória até a reinicialização; --secret vincula um nome a uma entrada criptografada do cofre, resolvida apenas enquanto ele estiver desbloqueado.',
'Lista apenas os nomes das variáveis. Para ler um valor explicitamente, use echo "$NOM".'],
'pt-PT':[
'As referências usernameSecret e tokenSecret leem o cofre desbloqueado durante a autenticação. clone aceita --username-secret NOME --token-secret NOME. Os ficheiros de credenciais continuam compatíveis. config --unset CHAVE remove uma referência.',
'Partilha uma variável pública na memória até ao reinício; --secret associa um nome a uma entrada cifrada do cofre, resolvida apenas enquanto o cofre estiver desbloqueado.',
'Lista apenas os nomes das variáveis. Para ler um valor explicitamente, use echo "$NOM".'],
'id-ID':[
'Referensi usernameSecret dan tokenSecret membaca brankas yang terbuka saat autentikasi. clone menerima --username-secret NAMA --token-secret NAMA. Berkas kredensial tetap didukung. config --unset KUNCI menghapus referensi.',
'Membagikan variabel publik dalam memori hingga dimulai ulang; --secret menautkan nama ke entri brankas terenkripsi, yang hanya dapat dibaca saat brankas terbuka.',
'Hanya mencantumkan nama variabel. Untuk membaca nilai dengan sengaja, gunakan echo "$NOM".'],
'ru-RU':[
'Ссылки usernameSecret и tokenSecret читают разблокированное хранилище при аутентификации. clone принимает --username-secret ИМЯ --token-secret ИМЯ. Файлы учётных данных по-прежнему поддерживаются. config --unset КЛЮЧ удаляет ссылку.',
'Публикует переменную в памяти до перезапуска; --secret связывает имя с зашифрованной записью, доступной только при разблокированном хранилище.',
'Выводит только имена переменных. Чтобы намеренно прочитать значение, используйте echo "$NOM".'],
'zh-CN':[
'usernameSecret 和 tokenSecret 引用在身份验证时读取已解锁的保险库。clone 支持 --username-secret 名称 --token-secret 名称。仍支持凭据文件。config --unset 键删除引用。',
'在内存中共享公开变量，直到重启；--secret 将名称绑定到保险库中的加密条目，仅在保险库解锁时解析。',
'只列出变量名称。如需主动读取某个值，请使用 echo "$NOM"。'],
'ja-JP':[
'usernameSecret と tokenSecret の参照は、認証時にロック解除済みの保管庫を読み取ります。clone は --username-secret 名前 --token-secret 名前を受け付けます。認証情報ファイルも引き続き使用できます。config --unset キーで参照を削除します。',
'再起動まで公開変数をメモリ内で共有します。--secret は名前を保管庫の暗号化された項目に関連付け、保管庫のロック解除中にのみ値を取得します。',
'変数名だけを一覧表示します。値を明示的に読むには echo "$NOM" を使用します。'],
'ko-KR':[
'usernameSecret 및 tokenSecret 참조는 인증할 때 잠금 해제된 보관함을 읽습니다. clone은 --username-secret 이름 --token-secret 이름을 지원합니다. 자격 증명 파일도 계속 지원합니다. config --unset 키로 참조를 제거합니다.',
'재시작할 때까지 메모리에서 공개 변수를 공유합니다. --secret은 이름을 보관함의 암호화된 항목에 연결하며, 보관함이 잠금 해제된 동안에만 값을 가져옵니다.',
'변수 이름만 나열합니다. 값을 명시적으로 읽으려면 echo "$NOM"을 사용하세요.'],
'ar':[
'تقرأ المرجعيات usernameSecret وtokenSecret الخزنة المفتوحة عند المصادقة. يقبل clone الخيارين --username-secret NAME و--token-secret NAME. تبقى ملفات بيانات الاعتماد مدعومة. يحذف config --unset KEY مرجعية.',
'يشارك متغيرًا عامًا في الذاكرة حتى إعادة التشغيل؛ يربط --secret اسمًا بإدخال مشفر في الخزنة، ولا تُقرأ قيمته إلا عندما تكون الخزنة مفتوحة.',
'يسرد أسماء المتغيرات فقط. لقراءة قيمة عمدًا، استخدم echo "$NOM".']}
def entries(text):
    result={}
    for line in text.splitlines():
        line=line.strip().rstrip(',')
        if line.startswith('{"'):
            row=json.loads('['+line[1:-1]+']');result[row[0]]=row
    return result
current=entries((SKETCH/'Tab5ShellHelp.h').read_text(encoding='utf8'))
archive=RELEASE.parent/'Tab5OS_0.8.24_corrections.zip'
delta={}
with zipfile.ZipFile(archive) as z:
    base='Tab5OS_Ecriture_v0.8.24/Tab5OS_Arduino_v0.8.24_Ecriture/'
    previous=entries(z.read(base+'Tab5ShellHelp.h').decode('utf8'))
    for locale,values in ROWS.items():
        oldpack=json.loads(z.read(base+'sdcard/System/languages/'+locale+'.json'))['translations']
        old=oldpack[previous['git'][2]]
        assert old.count('tab5.credentialFile')==1
        separator='。' if locale in ('zh-CN','ja-JP') else '. '
        prefix=old.rstrip('。.' ).rsplit(separator,1)[0]+separator
        assert prefix in old
        git=prefix.replace('tab5.credentialFile','tab5.credentialFile|tab5.usernameSecret|tab5.tokenSecret')+values[0]
        if locale=='fr-FR':assert git==current['git'][2],(git,current['git'][2])
        delta[locale]={current['git'][2]:git,current['export'][2]:values[1],current['env'][2]:values[2]}
(HERE/'delta-shell-0825.json').write_text(json.dumps(delta,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('Prepared 3 shell help keys in',len(delta),'languages; old Git paragraphs preserved')
