"""Discover the current release; no copied version paths in active tools."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
_candidates=[p for p in ROOT.glob('Tab5OS_Arduino_v*_Ecriture') if (p/(p.name+'.ino')).is_file()]
if len(_candidates)!=1:raise RuntimeError('Expected exactly one source sketch')
SKETCH=_candidates[0].name
VERSION=re.search(r'APP_VERSION\[\]\s*=\s*"([^"]+)"',(_candidates[0]/(SKETCH+'.ino')).read_text())[1]
TAG=VERSION.replace('.','')
BASELINE_VERSION='0.8.52'
BASELINE_TAG=BASELINE_VERSION.replace('.','')
BASELINE_ARCHIVE_SHA256='d603cf2c465a5ffecffbde90ba90bc158bdab52d770a0cc4337184298c26a696'
GENERATED_SKETCH_PREFIX='tools/native-apps/image-editor/test-output/'
GENERATED_RELEASE_PREFIX=SKETCH+'/'+GENERATED_SKETCH_PREFIX
FACTORY_PREFIX=SKETCH+'/sdcard/Factory/'
def obsolete_factory(relative):return relative.startswith(FACTORY_PREFIX) and not relative.startswith(FACTORY_PREFIX+'Tab5OS-'+VERSION+'/')
BASELINE_MANIFEST_SHA256='9d95df98c70ab6969f642769eb4ac8c86603929a874f02aaba2489a1176dfe31'
