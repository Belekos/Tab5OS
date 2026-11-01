"""Reviewed late .24 keys, separate from the fixed 82-row translation batches."""
from pathlib import Path
import json

HERE = Path(__file__).resolve().parent
RELEASE = HERE.parents[1]
SKETCH = next(RELEASE.glob('Tab5OS_Arduino_*_Ecriture'))
KEY = 'Migration vers System incomplète : relancez la récupération SD'
VALUES = {
    'fr-FR': KEY,
    'en-US': 'Migration to System incomplete: run SD recovery again',
    'de-DE': 'Migration nach System unvollständig: SD-Wiederherstellung erneut starten',
    'nl-NL': 'Migratie naar System onvolledig: voer SD-herstel opnieuw uit',
    'es-ES': 'Migración a System incompleta: vuelva a ejecutar la recuperación de la SD',
    'it-IT': 'Migrazione a System incompleta: eseguire nuovamente il ripristino della SD',
    'pt-BR': 'Migração para System incompleta: execute novamente a recuperação do SD',
    'pt-PT': 'Migração para System incompleta: execute novamente a recuperação do SD',
    'id-ID': 'Migrasi ke System belum selesai: jalankan kembali pemulihan SD',
    'ru-RU': 'Перенос в System не завершён: повторно запустите восстановление SD',
    'zh-CN': '迁移到 System 尚未完成：请重新运行 SD 卡恢复',
    'ja-JP': 'System への移行が完了していません。SD カードの復旧を再実行してください',
    'ko-KR': 'System으로의 이전이 완료되지 않았습니다. SD 카드 복구를 다시 실행하세요',
    'ar': 'الترحيل إلى System غير مكتمل: أعد تشغيل استعادة بطاقة SD',
}
delta = {locale: {KEY: value} for locale, value in VALUES.items()}
(HERE/'delta-late-0824.json').write_text(json.dumps(delta, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
for locale, additions in delta.items():
    path = SKETCH/'sdcard/System/languages'/(locale+'.json')
    pack = json.loads(path.read_text(encoding='utf-8'))
    pack['translations'].update(additions)
    path.write_text(json.dumps(pack, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print('Applied late migration key in 14 packs')
