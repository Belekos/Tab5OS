"""One-time explicit adaptation of the immutable .25 gate for .26."""
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
path = TOOLS / 'verify_features_0826.py'
assert not path.exists(), 'Do not overwrite the working .26 gate'
text = (TOOLS / 'verify_features_0825.py').read_text(encoding='utf8')
text = text.replace('import feature_contracts_0825 as additions',
                    'import feature_contracts_0825 as previous_additions\nimport feature_contracts_0826 as additions')
text = text.replace("VERSION='0.8.25'", "VERSION='0.8.26'")
text = text.replace("HISTORY_SHA256='9d257a209c2289b691ce8ebdeafe420901bbe6faafe1d676af65ad09c3189261'",
                    "HISTORY_SHA256='68e7ad2b73bfcf46a5968010f7f8151e841dd14df8578b5eeddccdc319324568'")
text = text.replace("NEW_REPORTS=(*additions.NEW_REPORTS,'help-0825.json')",
                    "NEW_REPORTS=(*previous_additions.NEW_REPORTS,'help-0825.json',*additions.NEW_REPORTS)")
text = text.replace('(?:19|20|21|22|23|24)', '(?:19|20|21|22|23|24|25)')
text = text.replace('Tab5OS_Arduino_v0.8.25_Ecriture', 'Tab5OS_Arduino_v0.8.26_Ecriture')
text = text.replace("'-0825',name", "'-0826',name")
text = text.replace("('0824','0823')", "('0825','0824','0823')")
text = text.replace("history=history or read_json(release/'tools/verification/history-0824/index.json')",
                    "history=read_json(release/'tools/verification/history-0824/index.json')")
text = text.replace("ledger=reports/'history-0824/index.json'", "ledger=reports/'history-0825/index.json'")
text = text.replace("history['baselineVersion']=='0.8.24'", "history['baselineVersion']=='0.8.25'")
text = text.replace("report_hashes={'history-0824/index.json':HISTORY_SHA256,",
                    "report_hashes={'history-0825/index.json':HISTORY_SHA256,\n                  'history-0824/index.json':digest(reports/'history-0824/index.json'),")
text = text.replace("key.endswith('-0825.json')", "key.endswith('-0826.json')")
text = text.replace("reference-final-delta-0825.json", "reference-final-delta-0826.json")
text = text.replace("target=release/'tools/verification/features-0825.json'", "target=release/'tools/verification/features-0826.json'")
text = text.replace('Current report must identify .25', 'Current report must identify .26')
text = text.replace('Historical report differs from .24 archive', 'Historical report differs from .25 archive')
text = text.replace('Historical .24 evidence index changed', 'Historical .25 evidence index changed')
text = text.replace("'Actual .25 executions plus exact archived .22/.23/.24 reports", "'Actual .26 executions plus exact archived .22/.23/.24/.25 reports")
text = text.replace('"""Verify .25 while preserving the exact .22/.23/.24 contracts and evidence.',
                    '"""Verify .26 while preserving the exact .22/.23/.24/.25 contracts and evidence.')
path.write_text(text, encoding='utf8')
print(path)
