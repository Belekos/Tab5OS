#!/usr/bin/env python3
"""Interface PC sans dépendance externe : OpenStreetMap / OpenTopoMap vers Tab5OS."""
from __future__ import annotations
import json
import os
import signal
import tempfile
from pathlib import Path
import queue
import subprocess
import sys
import threading
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, simpledialog
import webbrowser
import download_t5map as engine


class App:
    def __init__(self, root):
        self.root = root
        self.root.title('Cartes Tab5 — OpenStreetMap / OpenTopoMap')
        self.root.geometry('930x760')
        self.events = queue.Queue(); self.process = None; self.matches = []; self.cancel_file = None
        self.preferences = Path.home() / '.tab5maps' / 'pc-settings.json'
        try:
            data = json.loads(self.preferences.read_text(encoding='utf-8'))
        except (OSError, ValueError):
            data = {}
        if not isinstance(data, dict):
            data = {}
        self.endpoints = data.get('endpoints', {}) if isinstance(data.get('endpoints'), dict) else {}
        self.selected_provider = data.get('provider', 'OpenTopoMap')
        self.endpoints.setdefault(self.selected_provider, data.get('endpoint', ''))
        self.values = {key: tk.StringVar(value=data.get(key, default)) for key, default in {
            'provider': 'OpenTopoMap', 'city': '', 'lat': '0', 'lon': '0', 'radius': '100', 'low': '0', 'high': '12',
            'output': str(Path.home() / 'Tab5Maps'), 'name': 'ma-zone', 'sd': '', 'endpoint': '', 'max_tiles': '250000', 'rate': '0.5'}.items()}
        body = ttk.Frame(root, padding=14); body.pack(fill='both', expand=True)
        body.columnconfigure(1, weight=1); body.columnconfigure(3, weight=1)
        ttk.Label(body, text='Carte').grid(row=0, column=0, sticky='w')
        choose = ttk.Combobox(body, values=['OpenTopoMap', 'OpenStreetMap'], textvariable=self.values['provider'], state='readonly')
        choose.grid(row=0, column=1, sticky='ew', padx=6); choose.bind('<<ComboboxSelected>>', self.changed)
        self.osm_button = ttk.Button(body, text='Préparer OSM sur ce PC…', command=self.setup_osm)
        self.osm_button.grid(row=0, column=2, columnspan=2, sticky='ew')
        ttk.Label(body, text='Ville').grid(row=1, column=0, sticky='w', pady=8)
        ttk.Entry(body, textvariable=self.values['city']).grid(row=1, column=1, columnspan=2, sticky='ew', padx=6)
        ttk.Button(body, text='Rechercher', command=self.search).grid(row=1, column=3, sticky='ew')
        self.cities = ttk.Combobox(body, state='readonly'); self.cities.grid(row=2, column=0, columnspan=4, sticky='ew'); self.cities.bind('<<ComboboxSelected>>', self.city_selected)
        for row, (label, key, label2, key2) in enumerate([('Latitude', 'lat', 'Longitude', 'lon'), ('Rayon (km)', 'radius', 'Nom du paquet', 'name'), ('Zoom minimum', 'low', 'Zoom maximum', 'high')], start=3):
            ttk.Label(body, text=label).grid(row=row, column=0, sticky='w', pady=8)
            ttk.Entry(body, textvariable=self.values[key]).grid(row=row, column=1, sticky='ew', padx=6)
            ttk.Label(body, text=label2).grid(row=row, column=2, sticky='w')
            ttk.Entry(body, textvariable=self.values[key2]).grid(row=row, column=3, sticky='ew', padx=6)
        ttk.Label(body, text='Dossier PC').grid(row=6, column=0, sticky='w', pady=8)
        ttk.Entry(body, textvariable=self.values['output']).grid(row=6, column=1, columnspan=2, sticky='ew', padx=6)
        ttk.Button(body, text='Choisir…', command=lambda: self.directory('output')).grid(row=6, column=3, sticky='ew')
        ttk.Label(body, text='Carte SD (facultatif)').grid(row=7, column=0, sticky='w', pady=8)
        ttk.Entry(body, textvariable=self.values['sd']).grid(row=7, column=1, columnspan=2, sticky='ew', padx=6)
        ttk.Button(body, text='Choisir…', command=lambda: self.directory('sd')).grid(row=7, column=3, sticky='ew')
        self.explanation = ttk.Label(body, wraplength=870); self.explanation.grid(row=8, column=0, columnspan=4, sticky='w', pady=12)
        speed = ttk.LabelFrame(body, text='Vitesse de téléchargement'); speed.grid(row=9, column=0, columnspan=4, sticky='ew', pady=(0, 10)); speed.columnconfigure(1, weight=1)
        ttk.Label(speed, text='Requêtes par seconde (0,1–100)').grid(row=0, column=0, padx=5, pady=7)
        self.rate_entry = ttk.Spinbox(speed, from_=0.1, to=100, increment=0.1, textvariable=self.values['rate'], width=12)
        self.rate_entry.grid(row=0, column=1, sticky='w', padx=5)
        self.rate_hint = ttk.Label(speed, text='', wraplength=870); self.rate_hint.grid(row=1, column=0, columnspan=2, sticky='w', padx=5, pady=(0, 7))
        self.values['rate'].trace_add('write', self.rate_changed)
        self.values['endpoint'].trace_add('write', self.rate_changed)
        advanced = ttk.LabelFrame(body, text='Source et volume'); advanced.grid(row=10, column=0, columnspan=4, sticky='ew'); advanced.columnconfigure(1, weight=1)
        ttk.Label(advanced, text='URL XYZ (vide = source prévue)').grid(row=0, column=0, padx=5, pady=5)
        ttk.Entry(advanced, textvariable=self.values['endpoint']).grid(row=0, column=1, sticky='ew', padx=5)
        ttk.Label(advanced, text='Nombre maximal de tuiles').grid(row=1, column=0, padx=5, pady=5)
        ttk.Entry(advanced, textvariable=self.values['max_tiles']).grid(row=1, column=1, sticky='ew', padx=5)
        controls = ttk.Frame(body); controls.grid(row=11, column=0, columnspan=4, sticky='ew', pady=12)
        ttk.Button(controls, text='Estimer', command=lambda: self.download(True)).pack(side='left', padx=4)
        ttk.Button(controls, text='Télécharger / reprendre', command=lambda: self.download(False)).pack(side='left', padx=4)
        ttk.Button(controls, text='Pause', command=self.pause).pack(side='left', padx=4)
        ttk.Button(controls, text='Guide', command=self.guide).pack(side='right')
        self.log = tk.Text(body, height=4, wrap='word', state='disabled'); self.log.grid(row=12, column=0, columnspan=4, sticky='nsew'); body.rowconfigure(12, weight=1)
        self.changed(); self.root.after(100, self.poll); self.root.protocol('WM_DELETE_WINDOW', self.close)

    def changed(self, event=None):
        provider = self.values['provider'].get()
        if event is not None and provider != self.selected_provider:
            self.endpoints[self.selected_provider] = self.values['endpoint'].get()
            self.values['endpoint'].set(self.endpoints.get(provider, ''))
        self.selected_provider = provider
        self.osm_button.configure(state='normal' if provider == 'OpenStreetMap' else 'disabled')
        if self.values['provider'].get() == 'OpenStreetMap':
            text = 'OpenStreetMap Carto : tuiles calculées sur votre PC après import régional. Le bouton Préparer OSM utilise Docker Desktop. Les tuiles du serveur public OSM sont destinées à la consultation.'
        else:
            text = 'OpenTopoMap fonctionne directement, sans compte ni Docker. Serveur public à débit modéré (1 tuile / 2 s). Les grandes zones peuvent prendre plusieurs jours. Une URL locale/autorisée permet de préparer davantage de tuiles. Arrêt automatique si le serveur refuse les requêtes.'
        self.explanation.configure(text=text + ' Zoom 0–22 : au-delà des détails disponibles, la tablette agrandit les tuiles parentes importées.')
        self.rate_changed()

    def rate_changed(self, *_):
        from urllib.parse import urlparse
        template = self.values['endpoint'].get().strip()
        public_topo = self.values['provider'].get() == 'OpenTopoMap' and not template
        host = (urlparse(template).hostname or '').lower()
        public_topo = public_topo or host == 'tile.opentopomap.org' or host.endswith('.tile.opentopomap.org')
        try:
            requested = float(self.values['rate'].get().replace(',', '.'))
            effective = engine.request_rate(requested, public_topo)
            self.rate_hint.configure(text=('Cadence appliquée : ' + format(effective, 'g') + ' requête(s)/s.' +
                (' Limite du serveur OpenTopoMap : 0,5 requête/s.' if public_topo else '')))
        except (ValueError, OverflowError):
            self.rate_hint.configure(text='Saisir une fréquence entre 0,1 et 100 requêtes/s.')

    def write(self, text):
        self.log.configure(state='normal'); self.log.insert('end', text.rstrip() + '\n')
        if int(self.log.index('end-1c').split('.')[0]) > 250:
            self.log.delete('1.0', '80.0')
        self.log.see('end'); self.log.configure(state='disabled')

    def directory(self, key):
        path = filedialog.askdirectory()
        if path: self.values[key].set(path)

    def search(self):
        query = self.values['city'].get().strip()
        if not query: return
        self.write('Recherche de ville…')
        def work():
            try: self.events.put(('cities', engine.search_city(query)))
            except Exception as error: self.events.put(('line', str(error)))
        threading.Thread(target=work, daemon=True).start()

    def city_selected(self, event=None):
        selected = self.cities.current()
        if selected >= 0:
            city = self.matches[selected]; self.values['lat'].set(str(city['lat'])); self.values['lon'].set(str(city['lon']))

    def save(self):
        self.preferences.parent.mkdir(parents=True, exist_ok=True)
        self.endpoints[self.selected_provider] = self.values['endpoint'].get()
        data = {k: v.get() for k,v in self.values.items()}
        data['endpoints'] = self.endpoints
        temporary = self.preferences.with_suffix('.json.part')
        temporary.write_text(json.dumps(data, ensure_ascii=False), encoding='utf-8')
        os.replace(temporary, self.preferences)

    def download(self, estimate):
        if self.process and self.process.poll() is None:
            messagebox.showinfo('En cours', 'Mettre en pause le travail en cours avant de le modifier.'); return
        name = self.values['name'].get().strip()
        if not name or Path(name).name != name or any(c in name for c in '<>:"/\\|?*'):
            messagebox.showerror('Nom', 'Choisir un simple nom de fichier.'); return
        self.save()
        provider = 'osm' if self.values['provider'].get() == 'OpenStreetMap' else 'opentopomap'
        command = [sys.executable, '-u', str(Path(__file__).with_name('download_t5map.py')), str(Path(self.values['output'].get()) / (name + '-' + provider + '.t5map')), '--provider', provider, '--center', self.values['lat'].get()+','+self.values['lon'].get(), '--radius-km', self.values['radius'].get(), '--min-zoom', self.values['low'].get(), '--max-zoom', self.values['high'].get(), '--max-tiles', self.values['max_tiles'].get(), '--requests-per-second', self.values['rate'].get().replace(',', '.'), '--yes']
        if self.values['endpoint'].get().strip(): command += ['--url-template', self.values['endpoint'].get().strip()]
        if self.values['sd'].get().strip(): command += ['--sd-root', self.values['sd'].get().strip()]
        if estimate: command.append('--estimate-only')
        elif not messagebox.askokcancel('Préparer la zone', 'Télécharger ou reprendre cette zone ?\nUtilisez Estimer pour vérifier le nombre de tuiles et le volume. Les PNG terminés sont conservés après une pause.'):
            return
        descriptor, filename = tempfile.mkstemp(prefix='tab5-map-cancel-')
        os.close(descriptor); Path(filename).unlink()
        self.cancel_file = Path(filename)
        command += ['--cancel-file',filename]
        self.start(command)

    def start(self, command):
        if self.process and self.process.poll() is None: return
        try:
            environment = dict(os.environ, PYTHONIOENCODING='utf-8')
            self.process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding='utf-8', errors='replace', bufsize=1, env=environment, start_new_session=(os.name != 'nt'), creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name == 'nt' else 0)
        except OSError as error:
            self.write(str(error)); return
        child = self.process
        def consume():
            for line in child.stdout: self.events.put(('line', line))
            code = child.wait()
            message = {0: 'Opération terminée.', 2: 'Opération incomplète : consulter le détail ci-dessus.', 130: 'Téléchargement interrompu. Le cache permet de reprendre.'}.get(code, 'Arrêt du programme — code ' + str(code))
            self.events.put(('line', message))
        threading.Thread(target=consume, daemon=True).start()

    def pause(self):
        if self.process and self.process.poll() is None:
            if self.cancel_file is not None:
                self.cancel_file.write_text('stop')
                self.write('Pause demandée. La requête en cours se termine ; les données déjà écrites sont conservées.')
            else:
                if os.name == 'nt':
                    subprocess.run(['taskkill', '/PID', str(self.process.pid), '/T', '/F'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                else:
                    os.killpg(self.process.pid, signal.SIGTERM)
                self.write('Pause demandée. Les tuiles terminées restent dans le cache. Relancer la même zone pour reprendre.')

    def setup_osm(self):
        if self.process and self.process.poll() is None: return
        if messagebox.askyesno('Données OpenStreetMap', 'Avez-vous déjà un fichier régional .osm.pbf sur votre PC ?\nNon : saisir une URL Geofabrik.'):
            source = filedialog.askopenfilename(filetypes=[('OpenStreetMap PBF', '*.osm.pbf')])
        else:
            webbrowser.open('https://download.geofabrik.de/')
            source = simpledialog.askstring('Région Geofabrik', 'URL HTTPS du fichier .osm.pbf couvrant toute votre zone :')
        if not source: return
        if not messagebox.askokcancel('Préparer OpenStreetMap Carto', 'Docker Desktop Linux doit être démarré.\n\nL’import peut être long. Prévoir au moins 40 Gio libres sur le disque Docker, plus pour une grande région. Limite : 12 Gio RAM, 4 threads.\n\nLe serveur sera accessible uniquement sur ce PC. Les données existantes sont conservées.\n\nLancer la préparation ?'):
            return
        descriptor, filename = tempfile.mkstemp(prefix='tab5-osm-cancel-')
        os.close(descriptor); Path(filename).unlink()
        self.cancel_file = Path(filename)
        self.start([sys.executable, '-u', str(Path(__file__).with_name('setup_osm_server.py')), '--pbf', source, '--yes', '--cancel-file', filename])

    def guide(self):
        webbrowser.open(Path(__file__).with_name('CARTES_PC.md').resolve().as_uri())

    def poll(self):
        try:
            while True:
                kind, value = self.events.get_nowait()
                if kind == 'cities':
                    self.matches = value; self.cities.configure(values=[v['name'] for v in value]); self.write(str(len(value)) + ' résultats. Choisir la ville dans la liste.')
                    if len(value) == 1: self.cities.current(0); self.city_selected()
                else: self.write(value)
        except queue.Empty: pass
        self.root.after(100, self.poll)

    def close(self):
        if self.process and self.process.poll() is None:
            if not messagebox.askokcancel('Fermer', 'Mettre en pause et fermer ?'): return
            self.pause()
        self.save(); self.root.destroy()


if __name__ == '__main__':
    root = tk.Tk(); App(root); root.mainloop()
