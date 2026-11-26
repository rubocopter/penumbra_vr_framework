#!/usr/bin/env python3
"""Experimental Windows-build overlay for Linux/Wine. Python standard library only."""
import argparse
import hashlib
import html
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import sys
import tempfile
import xml.etree.ElementTree as ET

BUILDS = {
    'overture': ('95acb863441a17e701af2cd1b1ef301c55c1ac620269a167275580eb6954a448',),
    'black_plague': ('fd316f7586737a63eba989ece2271280fe6a98582a1319fe2151385a3df97bff',
                     'db086cc7a4c7b10864de0febbe2d71a3e4eff1ec8d067811a6a59edc1c617196'),
    'requiem': ('b64232d751cee376e1384cfe5a4a81dbd7deddf03983cc11d0d0a34d5825eea2',
                '577d1d7780872cd6c5b99b45759cdc48feea486a1ccbf319e8f6cf0eaed54e955'),
}
ALUT = 'd81dea8e88e35c319f7f2d8aaeb14c63a4986131492d3df860d1f2c18b844590'
OPENAL = '606cf8b4c22c7ae00813585279ac045115907bc66d1483eb26355907021ea4b0'
RECEIPT = '.penumbravr-linux.json'
BACKUP_MARKER = '.penumbravr-backup-complete'
DEFAULTS = {'overture': 'config/default_settings.cfg', 'black_plague': 'config/default_settings.cfg',
            'requiem': 'expansion01/config/requiem_default_settings.cfg'}
USER = {'overture': 'Penumbra Overture/Episode1/settings.cfg',
        'black_plague': 'Penumbra/Black Plague/settings.cfg', 'requiem': 'Penumbra/Requiem/settings.cfg'}


def digest(path):
    with path.open('rb') as stream:
        value = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(chunk)
        return value.hexdigest()


def safe(root, relative):
    part = PurePosixPath(relative)
    if not relative or part.is_absolute() or '..' in part.parts or '\\' in relative or ':' in relative:
        raise ValueError(f'Unsafe path: {relative}')
    target = root.joinpath(*part.parts)
    for path in (target, *target.parents):
        if path.is_symlink():
            raise ValueError(f'Symlink is not allowed here: {path}')
    return target


def verify_package(root):
    if any(path.is_symlink() for path in root.rglob('*')):
        raise ValueError('Package contains a symlink')
    seen = set()
    for line in (root / 'SHA256SUMS.txt').read_text(encoding='utf-8').splitlines():
        match = re.fullmatch(r'([a-f0-9]{64})  (.+)', line)
        if not match:
            raise ValueError('Malformed package manifest')
        expected, relative = match.groups()
        path = safe(root, relative)
        if relative in seen or not path.is_file() or digest(path) != expected:
            raise ValueError(f'Package checksum mismatch or duplicate: {relative}')
        seen.add(relative)
    actual = {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()
              and p != root / 'SHA256SUMS.txt'}
    if seen != actual:
        raise ValueError('Package contains unlisted or missing files')


def parse_config(data):
    text = data.decode('utf-8-sig')
    if '<!DOCTYPE' in text.upper() or '<!ENTITY' in text.upper():
        raise ValueError('Unsupported configuration declarations')
    stripped = re.sub(r'<\?xml[^?]*\?>', '', text)
    return text, ET.fromstring('<PvrRoot>' + stripped + '</PvrRoot>')


def set_attribute(text, section, attribute, value):
    if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*', section) or not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*', attribute):
        raise ValueError('Invalid recorded configuration attribute')
    pattern = r'<' + re.escape(section) + r'\b[^>]*>'
    def replace(match):
        tag = match.group()
        attr = r'\s+' + re.escape(attribute) + r'\s*=\s*([\"\x27]).*?\1'
        if value is None:
            return re.sub(attr, '', tag)
        addition = f' {attribute}="{html.escape(value, quote=True)}"'
        if re.search(attr, tag):
            return re.sub(attr, lambda _: addition, tag)
        return re.sub(r'(/?>)$', lambda m: addition + m.group(), tag)
    # Skip comments when locating sections; native files have multiple top-level elements.
    parts = re.split(r'(<!--.*?-->)', text, flags=re.S)
    return ''.join(p if p.startswith('<!--') else re.sub(pattern, replace, p) for p in parts)


def configure(data, settings, languages, fallback):
    text, tree = parse_config(data)
    changes = []
    desired = {section: dict(values) for section, values in settings.items()}
    game = tree.findall('Game')
    if len(game) > 1:
        raise ValueError('Duplicate configuration section Game')
    if game and 'LanguageFile' in game[0].attrib and game[0].get('LanguageFile') not in languages:
        if fallback not in languages:
            raise ValueError('English fallback language is missing')
        desired.setdefault('Game', {})['LanguageFile'] = fallback
    for section, attributes in desired.items():
        nodes = tree.findall(section)
        if len(nodes) > 1:
            raise ValueError(f'Duplicate configuration section {section}')
        if not nodes:
            continue
        for attribute, applied in attributes.items():
            before = nodes[0].get(attribute)
            if before != applied:
                changes.append([section, attribute, before, applied])
                text = set_attribute(text, section, attribute, applied)
    result = text.encode('utf-8')
    parse_config(result)
    return result, changes


def restore_config(data, changes):
    text, tree = parse_config(data)
    for section, attribute, original, applied in changes:
        nodes = tree.findall(section)
        if len(nodes) > 1:
            raise ValueError(f'Duplicate configuration section {section}')
        if nodes and nodes[0].get(attribute) == applied:
            text = set_attribute(text, section, attribute, original)
    result = text.encode('utf-8')
    parse_config(result)
    return result


def apply_laa(path, expected):
    data = bytearray(path.read_bytes())
    offset = struct.unpack_from('<I', data, 60)[0]
    if data[:2] != b'MZ' or data[offset:offset+6] != b'PE\0\0L\x01':
        raise ValueError('Invalid x86 PE image')
    data[offset+22] |= 0x20
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError('LAA result does not match the pinned transformed build')
    path.write_bytes(data)


def copy_files(source, target):
    for path in source.rglob('*'):
        relative = path.relative_to(source).as_posix()
        destination = safe(target, relative)
        if path.is_symlink():
            raise ValueError(f'Symlink payload: {path}')
        if path.is_file():
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, destination)
            if digest(path) != digest(destination):
                raise ValueError(f'Installed payload checksum mismatch: {relative}')


def atomic_write(path, data):
    descriptor, name = tempfile.mkstemp(prefix=path.name + '.penumbravr-', dir=path.parent)
    temp = Path(name)
    try:
        with os.fdopen(descriptor, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp, path)
    finally:
        if temp.exists():
            temp.unlink()


def verify_backup(root):
    marker = safe(root, BACKUP_MARKER)
    if not marker.is_file():
        raise ValueError('Backup is incomplete or predates this helper; inspect it before manual recovery')
    expected = json.loads(marker.read_text())
    if any(p.is_symlink() for p in root.rglob('*')):
        raise ValueError('Backup contains a symlink; restore aborted')
    saved = {p.relative_to(root).as_posix(): digest(p) for p in root.rglob('*')
             if p.is_file() and p != marker}
    if not isinstance(expected, dict) or saved != expected:
        raise ValueError('Backup changed after installation; restore aborted')


def x64(path):
    with path.open('rb') as stream:
        header = stream.read(64)
        if len(header) != 64 or header[:2] != b'MZ':
            raise ValueError('Neural model/helper must be a Windows x64 PE')
        stream.seek(struct.unpack_from('<I', header, 60)[0])
        if stream.read(6) != b'PE\0\0d\x86':
            raise ValueError('Neural model/helper must be x64')


def run(args, package):
    root = Path(args.game_root).absolute()
    redist = safe(root, 'redist')
    backup = safe(root, 'redist.before-penumbravr')
    displaced = safe(root, 'redist.penumbravr-test')
    if args.mode == 'restore':
        if not backup.exists() and redist.is_dir() and displaced.is_dir() and (redist / BACKUP_MARKER).is_file():
            verify_backup(redist)
            if not args.plan:
                (redist / BACKUP_MARKER).unlink()
            print('Original directory already restored; completed interrupted restore cleanup.')
            return
        if not backup.is_dir():
            raise ValueError('No original backup found')
        verify_backup(backup)
        interrupted = not redist.exists() and displaced.is_dir()
        if not interrupted and (not redist.is_dir() or displaced.exists()):
            raise ValueError('Conflicting restore directories; inspect previous installation first')
        records = []
        receipt = safe(displaced if interrupted else redist, RECEIPT)
        if receipt.is_file():
            state = json.loads(receipt.read_text())
            if state.get('documents'):
                if not args.documents or str(Path(args.documents).absolute()) != state['documents']:
                    raise ValueError('Restore requires the same --documents directory used at installation')
                for record in state['user_settings']:
                    path = safe(Path(state['documents']), USER[record['game']])
                    if path.is_file():
                        data = restore_config(path.read_bytes(), record['changes'])
                        # Do not leave preferences pointing to a translation being removed.
                        game = record['game']
                        directory = backup / ('expansion01/config' if game == 'requiem' else 'config')
                        languages = {p.name for p in directory.glob('*.lang')}
                        fallback = 'English_exp.lang' if game == 'requiem' else 'English.lang'
                        data, _ = configure(data, {}, languages, fallback)
                        records.append((path, path.read_bytes(), data))
        if args.plan:
            print(f'Restore {backup}; conditionally restore {len(records)} user settings files')
            return
        written = []
        try:
            for path, before, after in records:
                atomic_write(path, after)
                written.append((path, before))
            if not interrupted:
                redist.rename(displaced)
            try:
                backup.rename(redist)
            except BaseException:
                if not interrupted:
                    displaced.rename(redist)
                raise
        except BaseException:
            for path, before in reversed(written):
                atomic_write(path, before)
            raise
        (redist / BACKUP_MARKER).unlink()
        print(f'Original files restored. Test the game before deleting {displaced}')
        return

    if not redist.is_dir():
        raise ValueError('Select the game directory containing redist/')
    if displaced.exists():
        raise ValueError('Previous redist.penumbravr-test exists; inspect it first')
    verify_package(package)
    game = json.loads((package / 'package-game.json').read_text())['game']
    if game not in ('overture', 'black_plague'):
        raise ValueError('Unknown package game')
    games = [game]
    if backup.exists():
        raise ValueError('Original backup already exists; restore before reinstalling')
    for path in redist.rglob('*'):
        if path.is_symlink():
            raise ValueError(f'Game tree contains a symlink: {path}')
    exe = safe(redist, 'Penumbra.exe')
    if digest(exe) not in BUILDS[game]:
        raise ValueError('Unsupported or modified Windows game executable')
    if game == 'black_plague':
        if digest(safe(redist, 'alut.dll')) != ALUT:
            raise ValueError('Unsupported or modified original ALUT')
        openal = safe(redist, 'OpenAL32.dll')
        if openal.exists() and digest(openal) != OPENAL:
            raise ValueError('Unknown original OpenAL32.dll')
        requiem = safe(redist, 'Requiem.exe')
        if requiem.exists():
            if digest(requiem) not in BUILDS['requiem']:
                raise ValueError('Unsupported Requiem executable')
            games.append('requiem')
    for relative in (RECEIPT, BACKUP_MARKER, '.penumbravr', '.penumbravr-neural', 'PenumbraVR.BlackPlague.install.json',
                     'PenumbraVR_alut_original.dll', 'PenumbraVR.BlackPlague.Probe.dll',
                     'PenumbraVR.Requiem.Probe.dll', 'PenumbraVR_OpenALSoft.dll', 'Penumbra_vr.exe',
                     'openvr_api.dll', 'vr', 'assets/rework', 'PenumbraVR-Neural'):
        if safe(redist, relative).exists():
            raise ValueError(f'Existing mod installation: {relative}')
    profile = json.loads((package / 'installer-game-profile.json').read_text())
    documents = Path(args.documents).absolute() if args.documents else None
    config_plans = []
    for current in games:
        directory = 'expansion01/config' if current == 'requiem' else 'config'
        fallback = 'English_exp.lang' if current == 'requiem' else 'English.lang'
        if not safe(redist, directory + '/' + fallback).is_file():
            raise ValueError(f'Missing original language data: {current}')
        languages = {p.name for p in (redist / directory).glob('*.lang')}
        if args.spanish:
            languages.add('Espanol_exp.lang' if current == 'requiem' else 'Espanol.lang')
        settings = {}
        if args.recommended_settings:
            for source in (profile['common'], profile['games'][current]):
                for section, attributes in source.items():
                    settings.setdefault(section, {}).update(attributes)
        paths = [(safe(redist, DEFAULTS[current]), False)]
        if documents:
            paths.append((safe(documents, USER[current]), True))
        for path, personal in paths:
            if path.exists():
                before = path.read_bytes()
                after, changes = configure(before, settings, languages, fallback)
                if changes:
                    config_plans.append((path, before, after, current, personal, changes))
    if args.textures and game != 'overture':
        raise ValueError('Texture enhancements are supplied for Overture only')
    required = ['redist/vr/actions.json', 'optional/alsoft.ini']
    if args.spanish:
        required.append('optional/spanish/config/Espanol.lang')
        if 'requiem' in games:
            required.append('optional/spanish/expansion01/config/Espanol_exp.lang')
    if args.textures:
        selection = json.loads((package / 'texture-selection.json').read_text())
        required.extend('optional/textures/' + item['Path'] for item in selection['Files'])
    for relative in required:
        if not safe(package, relative).is_file():
            raise ValueError(f'Missing selected payload: {relative}')
    if args.neural_model:
        model = Path(args.neural_model).absolute()
        if model.name.lower() != 'nvngx_dlssnr.dll':
            raise ValueError('Select your local nvngx_dlssnr.dll')
        for path in (model, package / 'optional/neural/PenumbraNeuralHost.exe',
                     package / 'optional/neural/nvngx.dll_nr_bridge.dll'):
            x64(path)
        if not args.wine_redist or not re.match(r'^[A-Za-z]:\\', args.wine_redist) or any(c in args.wine_redist for c in '\r\n'):
            raise ValueError('Neural import requires --wine-redist with the Wine-visible Windows redist path')
    if args.plan:
        print(f'Install {", ".join(games)}; Spanish={args.spanish}, textures={args.textures}, '
              f'recommended settings={args.recommended_settings}, LAA={args.laa}, Neural={bool(args.neural_model)}')
        print(f'Backup: {backup}; changed configuration files: {len(config_plans)}')
        return
    # Backup completes before any modification. A failed backup is retained for inspection.
    originals = {p.relative_to(redist).as_posix(): digest(p) for p in redist.rglob('*') if p.is_file()}
    shutil.copytree(redist, backup)
    saved = {p.relative_to(backup).as_posix(): digest(p) for p in backup.rglob('*') if p.is_file()}
    if saved != originals:
        raise ValueError('Backup verification failed; original game was not modified')
    atomic_write(backup / BACKUP_MARKER, json.dumps(originals).encode())
    written = []
    try:
        if game == 'black_plague':
            shutil.copy2(backup / 'alut.dll', redist / 'PenumbraVR_alut_original.dll')
        copy_files(package / 'redist', redist)
        if args.spanish:
            copy_files(package / 'optional/spanish/config', redist / 'config')
            if 'requiem' in games:
                copy_files(package / 'optional/spanish/expansion01', redist / 'expansion01')
        if args.textures:
            copy_files(package / 'optional/textures', redist)
        if not (redist / 'alsoft.ini').exists():
            shutil.copy2(package / 'optional/alsoft.ini', redist / 'alsoft.ini')
        if args.laa and game == 'black_plague':
            for current in games:
                apply_laa(redist / ('Requiem.exe' if current == 'requiem' else 'Penumbra.exe'), BUILDS[current][1])
        if args.neural_model:
            neural = redist / 'PenumbraVR-Neural'
            (neural / 'NeuralRuntime').mkdir(parents=True)
            shutil.copy2(package / 'optional/neural/PenumbraNeuralHost.exe', neural)
            shutil.copy2(package / 'optional/neural/nvngx.dll_nr_bridge.dll', neural / 'NeuralRuntime')
            shutil.copy2(model, neural / 'NeuralRuntime/nvngx_dlssnr.dll')
            win = args.wine_redist.rstrip('\\') + '\\PenumbraVR-Neural'
            (redist / 'NeuralPost.ini').write_bytes(
                f'[Neural]\r\nHostPath={win}\\PenumbraNeuralHost.exe\r\nRuntimeDirectory={win}\\NeuralRuntime\r\nTimeoutMs=20\r\n'.encode('utf-16'))
        changed_user = {c: changes for _, _, _, c, personal, changes in config_plans if personal}
        state = {'schema': 1, 'game': game, 'documents': str(documents) if documents else None,
                 'user_settings': [{'game': c, 'changes': changed_user.get(c, [])} for c in games]}
        # Journal external preference ownership before writing any user preference.
        atomic_write(redist / RECEIPT, json.dumps(state, indent=2).encode())
        for path, before, after, _, personal, _ in config_plans:
            if path.read_bytes() != before:
                raise ValueError(f'Configuration changed during installation: {path}')
            atomic_write(path, after)
            if personal:
                written.append((path, before))
    except BaseException:
        for path, before in reversed(written):
            atomic_write(path, before)
        redist.rename(displaced)
        backup.rename(redist)
        (redist / BACKUP_MARKER).unlink()
        raise
    print('Overlay installed. Start SteamVR, then launch through Steam Proton.')
    if not documents:
        print('Existing Wine user preferences were untouched; use --documents to include them on the next installation.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['install', 'restore'])
    parser.add_argument('game_root', help='Steam game directory containing redist/')
    parser.add_argument('--spanish', action='store_true', help='Install community translations; keep valid current language selection')
    parser.add_argument('--textures', action='store_true', help='Overture selected texture enhancements')
    parser.add_argument('--recommended-settings', action='store_true')
    parser.add_argument('--documents', help='Actual Wine/Proton Windows Documents directory (optional user settings scope)')
    parser.add_argument('--laa', action='store_true', help='Apply pinned BP/Requiem Large Address Aware transform')
    parser.add_argument('--neural-model', help='Optional experimental local x64 nvngx_dlssnr.dll; never downloaded')
    parser.add_argument('--wine-redist', help='Windows path of redist as seen inside the selected Wine prefix')
    parser.add_argument('--plan', action='store_true', help='Validate and preview without writing')
    args = parser.parse_args()
    try:
        run(args, Path(__file__).resolve().parent)
    except (OSError, ValueError, KeyError, ET.ParseError, struct.error) as error:
        parser.exit(1, f'No successful installation: {error}\n')


if __name__ == '__main__':
    main()
