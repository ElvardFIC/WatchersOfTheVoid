"""Read-only upstream package inspection; never installs or executes mod files."""
import argparse
import concurrent.futures
import hashlib
import io
import json
from pathlib import Path
import urllib.request
import zipfile

PACKAGES = [
    ('forder', 'EmoteWheel', None), ('NynrahGhost', 'Fusion', '0.8108.3'),
    ('Moddy', 'VEmu', None), ('forder', 'Multivoid_Female_Skin', None),
    ('TheiRog', 'JeepMod', None), ('ManAboutTown', 'FireTower', None),
    ('sometimesifeeellikeamotherlesschild', 'PhotographyPlus', None),
    ('Thunderstore', 'unreal_shimloader', None), ('Moddy', 'SmartTV', None),
    ('Hirokhai', 'BuyableJetpacks', None), ('Hirokhai', 'BuyableShovels', None),
]
OUT = Path(__file__).resolve().parents[2] / 'build/requested-mods-audit'
OFFLINE = False


def fetch(url):
    request = urllib.request.Request(url, headers={'User-Agent': 'Multivoid-compatibility-audit'})
    with urllib.request.urlopen(request, timeout=120) as response:
        return response.read()


def inspect(package):
    owner, name, pin = package
    target = OUT / f'{owner}-{name}'
    target.mkdir(parents=True, exist_ok=True)
    if OFFLINE:
        metadata = json.loads((target / 'metadata.json').read_text(encoding='utf-8'))
    else:
        metadata = json.loads(fetch(f'https://thunderstore.io/api/experimental/package/{owner}/{name}/'))
    (target / 'metadata.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
    version = pin or metadata['latest']['version_number']
    url = f'https://thunderstore.io/package/download/{owner}/{name}/{version}/'
    archive = target / f'{version}.zip'
    if not archive.exists():
        if OFFLINE:
            raise FileNotFoundError(archive)
        archive.write_bytes(fetch(url))
    raw = archive.read_bytes()
    with zipfile.ZipFile(io.BytesIO(raw)) as bundle:
        entries = [{'path': entry.filename, 'size': entry.file_size} for entry in bundle.infolist()]
        texts = {}
        for entry in bundle.infolist():
            path = Path(entry.filename)
            if path.suffix.lower() in ('.json', '.md', '.lua', '.ini', '.txt', '.cfg') and entry.file_size < 2_000_000:
                texts[entry.filename] = bundle.read(entry).decode('utf-8-sig', errors='replace')
        manifest = next((json.loads(value) for key, value in texts.items() if key.lower() == 'manifest.json'), {})
    if manifest.get('version_number') != version or manifest.get('name') != name:
        raise ValueError(f'{owner}-{name}: archive manifest does not match requested package/version')
    findings = []
    if name == 'EmoteWheel':
        script = texts.get('mod/Scripts/main.lua', '')
        if 'function net.broadcast(kind, id) end' in script and 'function net.poll() end' in script:
            findings.append('Confirmed empty network bridge; emotes are local only.')
    if name == 'VEmu':
        if any('Not SmartTV' in value for key, value in texts.items() if key.lower().endswith('readme.md')):
            findings.append('Upstream explicitly excludes SmartTV AV output.')
    if any(dep.startswith('Pelmentor-Multivoid-') for dep in manifest.get('dependencies', [])):
        findings.append('Original Multivoid dependency: inspect profile for a duplicate multiplayer DLL.')
    result = dict(package=f'{owner}-{name}', version=version, archive_sha256=hashlib.sha256(raw).hexdigest(),
                  url=url, manifest=manifest, entries=entries, texts=texts, findings=findings, runtime_verified=False)
    (target / 'inspection.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({key: result[key] for key in ('package', 'version', 'findings')}) , flush=True)
    return {key: result[key] for key in ('package', 'version', 'archive_sha256', 'url', 'manifest', 'findings', 'runtime_verified')}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--offline', action='store_true', help='Inspect cached metadata/ZIPs without network access.')
    parser.add_argument('--output-directory', type=Path, default=OUT)
    args = parser.parse_args()
    OUT, OFFLINE = args.output_directory.resolve(), args.offline
    OUT.mkdir(parents=True, exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(inspect, PACKAGES))
    (OUT / 'summary.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
