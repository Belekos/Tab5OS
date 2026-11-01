"""Prepare current delivery documents/scripts; historical release notes stay exact."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
for name in ('finalize_release_docs.py', 'audit_publication.py', 'package_release.py'):
    path = ROOT / 'tools' / name
    text = path.read_text(encoding='utf8').replace('0825', '0826').replace('0.8.25', '0.8.26')
    if name == 'package_release.py':
        text = text.replace('Tab5OS-0.8.24/', 'Tab5OS-0.8.25/')
    path.write_text(text, encoding='utf8')

for path in ROOT.iterdir():
    if not path.is_file() or path.suffix not in ('.md', '.txt', '.ps1'):
        continue
    if path.name.startswith(('NOUVEAUTES', 'AUDIT_', 'RTL_SDR_082', 'REGLAGES_082')):
        continue
    text = path.read_text(encoding='utf8')
    text = text.replace('Tab5OS_Arduino_v0.8.25_Ecriture', 'Tab5OS_Arduino_v0.8.26_Ecriture')
    text = text.replace('Tab5OS_Ecriture_v0.8.25', 'Tab5OS_Ecriture_v0.8.26')
    text = text.replace('Tab5OS_v0.8.25.bin', 'Tab5OS_v0.8.26.bin')
    text = text.replace('Factory/Tab5OS-0.8.25', 'Factory/Tab5OS-0.8.26')
    text = text.replace('NOUVEAUTES_v0.8.25.md', 'NOUVEAUTES_v0.8.26.md')
    text = re.sub(r'(?m)^# Tab5OS 0\.8\.25$', '# Tab5OS 0.8.26', text)
    text = re.sub(r'(?m)^## Nouveautés 0\.8\.25$', '## Nouveautés 0.8.26', text)
    text = re.sub(r'(<!-- build-status -->).*?(<!-- /build-status -->)',
                  r'\1\nCompilation complète 0.8.26 en attente de vérification.\n\2', text, flags=re.S)
    text = re.sub(r'(\[BUILD_STATUS\]).*?(\[/BUILD_STATUS\])',
                  r'\1\nThe complete 0.8.26 build is pending verification.\n\2', text, flags=re.S)
    path.write_text(text, encoding='utf8')
print('Current delivery paths and pending status prepared; historical notes unchanged')
