#!/usr/bin/env python3
"""Prépare le véritable style OpenStreetMap Carto sur le PC avec Docker Desktop."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import signal
import subprocess
import sys
import time
from urllib.parse import urlparse
from urllib.request import Request, urlopen

IMAGE = 'overv/openstreetmap-tile-server:latest'


def plan(source, instance=None, port=8080):
    remote = source.startswith('https://')
    if remote:
        parsed = urlparse(source)
        if not parsed.hostname or parsed.username or not parsed.path.endswith('.osm.pbf'):
            raise ValueError('URL HTTPS .osm.pbf requise')
    else:
        source = str(Path(source).expanduser().resolve())
        if not Path(source).is_file() or not source.endswith('.osm.pbf'):
            raise ValueError('Fichier .osm.pbf introuvable')
    instance = instance or hashlib.sha256(source.encode()).hexdigest()[:12]
    if not re.fullmatch(r'[a-z0-9][a-z0-9_-]{0,35}', instance) or not 1024 <= port <= 65535:
        raise ValueError('Instance ou port invalide')
    prefix = 'tab5-osm-' + instance
    database, tiles = prefix + '-data', prefix + '-tiles'
    settings = ['--memory', '12g', '--shm-size', '256m', '-e', 'THREADS=4', '-e', 'OSM2PGSQL_EXTRA_ARGS=-C 4096', '-v', database + ':/data/database/', '-v', tiles + ':/data/tiles/']
    data = ['-e', 'DOWNLOAD_PBF=' + source] if remote else ['--mount', 'type=bind,source=' + source + ',target=/data/region.osm.pbf,readonly']
    return {'source': source, 'container': prefix, 'database': database, 'tiles': tiles, 'port': port,
            'create': [['docker', 'volume', 'create', database], ['docker', 'volume', 'create', tiles]],
            'import': ['docker', 'run', '--rm', '--name', prefix + '-import'] + settings + data + [IMAGE, 'import'],
            'run': ['docker', 'run', '-d', '--name', prefix, '-p', '127.0.0.1:' + str(port) + ':80'] + settings + [IMAGE, 'run']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pbf', required=True, help='fichier régional ou URL HTTPS Geofabrik .osm.pbf')
    parser.add_argument('--instance', help='nouveau nom pour importer une autre région ou actualiser les données')
    parser.add_argument('--port', type=int, default=8080)
    parser.add_argument('--state-dir', type=Path, default=Path.home() / '.tab5maps' / 'servers')
    parser.add_argument('--cancel-file', type=Path, help=argparse.SUPPRESS)
    parser.add_argument('--plan-only', action='store_true')
    parser.add_argument('--yes', action='store_true')
    args = parser.parse_args()
    try:
        commands = plan(args.pbf, args.instance, args.port)
    except ValueError as error:
        parser.error(str(error))
    print('Style : OpenStreetMap Carto. Données : ' + commands['source'], flush=True)
    print('Docker Desktop (conteneurs Linux) requis. Import : 12 Gio RAM maximum, 4 threads ; prévoir au moins 40 Gio libres sur le disque Docker, davantage pour les grandes régions.', flush=True)
    print('La région .osm.pbf doit couvrir TOUT le rayon souhaité. Hors de cet extrait, le rendu ne contiendra pas les routes.', flush=True)
    print('Serveur accessible seulement sur ce PC : http://127.0.0.1:' + str(args.port), flush=True)
    for command in commands['create'] + [commands['import'], commands['run']]:
        print(subprocess.list2cmdline(command), flush=True)
    if args.plan_only:
        return 0
    if not args.yes and input('Télécharger les composants et importer ces données ? [o/N] ').lower().strip() not in ('o', 'oui', 'yes', 'y'):
        return 1
    def interrupted(signum, frame):
        subprocess.run(['docker', 'stop', '-t', '10', commands['container'] + '-import'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        raise SystemExit(130)
    signal.signal(signal.SIGINT, interrupted)
    if hasattr(signal, 'SIGTERM'):
        signal.signal(signal.SIGTERM, interrupted)
    subprocess.run(['docker', 'info', '--format', '{{.OSType}}'], check=True)
    args.state_dir.mkdir(parents=True, exist_ok=True)
    state_file = args.state_dir / (commands['container'] + '.json')
    state = json.loads(state_file.read_text()) if state_file.exists() else None
    if state and state.get('source') != commands['source']:
        parser.error('Cette instance contient une autre région. Utiliser un nouveau --instance.')
    if not state or not state.get('imported'):
        # Never erase a failed or unrelated database implicitly.
        for volume in (commands['database'], commands['tiles']):
            found = subprocess.run(['docker', 'volume', 'inspect', volume], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
            if found:
                parser.error('Volume déjà présent sans import terminé : ' + volume + '. Utiliser un nouveau --instance ; les données existantes sont conservées.')
        for command in commands['create']:
            subprocess.run(command, check=True)
        child = subprocess.Popen(commands['import'])
        while child.poll() is None:
            if args.cancel_file and args.cancel_file.exists():
                subprocess.run(['docker', 'stop', '-t', '10', commands['container'] + '-import'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                if child.poll() is None:
                    child.terminate()
                child.wait()
                print('Import interrompu. Les volumes sont conservés ; utiliser un nouveau nom d’instance pour recommencer.', flush=True)
                return 130
            time.sleep(0.2)
        if child.returncode:
            raise subprocess.CalledProcessError(child.returncode, commands['import'])
        state_file.write_text(json.dumps({'source': commands['source'], 'imported': True, 'port': args.port}), encoding='utf-8')
    exists = subprocess.run(['docker', 'container', 'inspect', commands['container']], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
    subprocess.run(['docker', 'start', commands['container']] if exists else commands['run'], check=True)
    print('OSM prêt. URL dans Cartes Tab5 : http://127.0.0.1:' + str(args.port) + '/tile/{z}/{x}/{y}.png', flush=True)
    print('Le premier rendu peut prendre du temps. Pour arrêter et libérer la RAM : docker stop ' + commands['container'], flush=True)
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, subprocess.CalledProcessError) as error:
        print('Préparation interrompue : ' + str(error), file=sys.stderr)
        raise SystemExit(2)
