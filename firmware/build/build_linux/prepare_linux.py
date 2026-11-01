"""Install pinned Tab5OS dependencies in a dedicated Linux build directory."""
import argparse, hashlib, json, os, shutil, subprocess, tempfile, urllib.request, zipfile
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--environment',required=True,type=Path);args=ap.parse_args()
    root=args.environment.resolve();root.mkdir(parents=True,exist_ok=True)
    cli=root/'arduino-cli'
    if not cli.is_file():raise RuntimeError('Install Arduino CLI 1.5.1 as environment/arduino-cli first')
    config=root/'arduino-cli.yaml'
    config.write_text('directories:\n  data: '+json.dumps(str(root/'data'))+'\n  downloads: '+json.dumps(str(root/'downloads'))+'\n  user: '+json.dumps(str(root/'sketchbook'))+'\nboard_manager:\n  additional_urls:\n    - https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json\n')
    env=os.environ.copy()
    if env.get('HTTPS_PROXY'):env['ARDUINO_NETWORK_PROXY']=env['HTTPS_PROXY']
    def run(*args):subprocess.run([str(cli),*args,'--config-file',str(config)],env=env,check=True)
    run('core','update-index');run('core','install','m5stack:esp32@3.3.9')
    run('lib','update-index')
    for lib in ['M5GFX@0.2.30','M5Unified@0.2.23','lvgl@9.6.0','ArduinoJson@7.4.3']:run('lib','install','--no-deps',lib)
    audio=root/'downloads/ESP8266Audio-2.4.2.zip'
    with urllib.request.urlopen('https://codeload.github.com/earlephilhower/ESP8266Audio/zip/refs/tags/2.4.2',timeout=120) as response:audio.write_bytes(response.read())
    expected='80e7a77de974e41f7cc73020067912b2eb38d991bcd170a9c3734c8ea61c7224'
    if hashlib.sha256(audio.read_bytes()).hexdigest()!=expected:raise RuntimeError('ESP8266Audio archive checksum mismatch')
    dest=root/'sketchbook/libraries/ESP8266Audio'
    if dest.is_symlink():raise RuntimeError('Refusing symlinked audio directory')
    with tempfile.TemporaryDirectory(dir=root,prefix='audio-') as temp:
        with zipfile.ZipFile(audio) as z:
            for member in z.namelist():
                path=Path(member)
                if path.is_absolute() or '..' in path.parts:raise RuntimeError('Unsafe archive member')
            z.extractall(temp)
        source=Path(temp)/'ESP8266Audio-2.4.2'
        if dest.exists():shutil.rmtree(dest)
        shutil.move(str(source),str(dest))
    print('Dependencies prepared:',root)
if __name__=='__main__':main()
