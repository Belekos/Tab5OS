from pathlib import Path
import json
HERE=Path(__file__).resolve().parent;SKETCH=next(HERE.parents[1].glob('Tab5OS_Arduino_*_Ecriture'))
ROWS={
'fr-FR':["Masquer sur l'accueil",'HTTP : connexion non chiffrée.'],
'en-US':['Hide on Home','HTTP: unencrypted connection.'],
'de-DE':['Auf der Startseite ausblenden','HTTP: unverschlüsselte Verbindung.'],
'es-ES':['Ocultar en Inicio','HTTP: conexión sin cifrar.'],
'it-IT':['Nascondi nella schermata iniziale','HTTP: connessione non cifrata.'],
'nl-NL':['Verbergen op Start','HTTP: onversleutelde verbinding.'],
'pt-BR':['Ocultar na tela inicial','HTTP: conexão sem criptografia.'],
'pt-PT':['Ocultar no ecrã inicial','HTTP: ligação não cifrada.'],
'id-ID':['Sembunyikan di Beranda','HTTP: koneksi tidak terenkripsi.'],
'ru-RU':['Скрыть на главном экране','HTTP: незашифрованное соединение.'],
'zh-CN':['在主页隐藏','HTTP：未加密的连接。'],
'ja-JP':['ホームで非表示','HTTP：暗号化されていない接続。'],
'ko-KR':['홈에서 숨기기','HTTP: 암호화되지 않은 연결.'],
'ar':['إخفاء في الرئيسية','HTTP: اتصال غير مشفر.']}
delta={}
for locale,values in ROWS.items():
    old=json.loads((SKETCH/'sdcard/System/languages'/(locale+'.json')).read_text(encoding='utf8'))['translations']
    delta[locale]=dict(zip(ROWS['fr-FR'],values));delta[locale]['actif']=old['Actif'][:1].lower()+old['Actif'][1:]
(HERE/'delta-indirect-0825.json').write_text(json.dumps(delta,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
