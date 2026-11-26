"""Filesystem tests; never launch or modify an installed game."""
import importlib.util
import json
from pathlib import Path
import tempfile
import shutil
import unittest
from argparse import Namespace
from unittest.mock import patch

SOURCE = Path(__file__).resolve().parents[2] / 'tools/install-linux.py'


class InstallerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location('linux_installer', SOURCE)
        cls.mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.mod)

    def test_config_preserves_unrelated_data_and_restores_conditionally(self):
        original = b'<!-- keep -->\n<Graphics LimitFPS="true" PostEffects="false"/>\n<Screen Vsync="true"/>\n<Game LanguageFile="Missing.lang"/>\n'
        updated, changes = self.mod.configure(original, {'Graphics': {'LimitFPS': 'false', 'FSAA': '0'}}, {'English.lang'}, 'English.lang')
        self.assertIn(b'<!-- keep -->', updated)
        self.assertIn(b'PostEffects="false"', updated)
        self.assertIn(b'LanguageFile="English.lang"', updated)
        edited = updated.replace(b'FSAA="0"', b'FSAA="4"')
        restored = self.mod.restore_config(edited, changes)
        self.assertIn(b'LimitFPS="true"', restored)
        self.assertIn(b'FSAA="4"', restored)
        self.assertIn(b'LanguageFile="Missing.lang"', restored)

    def test_duplicate_sections_fail(self):
        with self.assertRaises(ValueError):
            self.mod.configure(b'<Screen/><Screen/>', {'Screen': {'Vsync': 'false'}}, set(), 'English.lang')

    def test_package_tamper_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'data').write_bytes(b'changed')
            (root / 'SHA256SUMS.txt').write_text('0' * 64 + '  data\n')
            with self.assertRaises(ValueError):
                self.mod.verify_package(root)

    def test_manifest_traversal_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'SHA256SUMS.txt').write_text('0' * 64 + '  ../outside\n')
            with self.assertRaises(ValueError):
                self.mod.verify_package(root)

    def test_laa_requires_exact_result(self):
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / 'game.exe'
            data = bytearray(256)
            data[:2] = b'MZ'
            data[60:64] = (128).to_bytes(4, 'little')
            data[128:132] = b'PE\0\0'
            data[132:134] = (0x14c).to_bytes(2, 'little')
            target.write_bytes(data)
            with self.assertRaises(ValueError):
                self.mod.apply_laa(target, '0' * 64)
            self.assertEqual(target.read_bytes(), data)

    def fixture(self, root):
        package, game, documents = root / 'package', root / 'game', root / 'Documents'
        files = {
            'package-game.json': json.dumps({'game': 'overture'}),
            'installer-game-profile.json': json.dumps({'common': {'Graphics': {'LimitFPS': 'false'}}, 'games': {'overture': {}}}),
            'redist/Penumbra.exe': 'vr executable', 'redist/vr/actions.json': '{}',
            'optional/spanish/config/Espanol.lang': 'translation', 'optional/alsoft.ini': '[general]\nhrtf = auto\n',
        }
        for relative, data in files.items():
            path = package / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(data)
        (package / 'SHA256SUMS.txt').write_text(''.join(self.mod.digest(package / rel) + '  ' + rel + '\n' for rel in files))
        redist = game / 'redist'
        (redist / 'config').mkdir(parents=True)
        (redist / 'Penumbra.exe').write_bytes(b'original executable')
        (redist / 'config/English.lang').write_text('English')
        cfg = b'<Graphics LimitFPS="true"/>\n<Game LanguageFile="English.lang"/>\n'
        (redist / 'config/default_settings.cfg').write_bytes(cfg)
        personal = documents / self.mod.USER['overture']
        personal.parent.mkdir(parents=True)
        personal.write_bytes(cfg)
        args = Namespace(mode='install', game_root=str(game), documents=str(documents),
                         spanish=True, textures=False, recommended_settings=True, laa=False,
                         neural_model=None, wine_redist=None, plan=False)
        return package, game, personal, args

    def test_install_restore_and_later_preference_edit(self):
        with tempfile.TemporaryDirectory() as temp:
            package, game, personal, args = self.fixture(Path(temp))
            original = self.mod.digest(game / 'redist/Penumbra.exe')
            with patch.dict(self.mod.BUILDS, {'overture': (original,)}):
                self.mod.run(args, package)
            self.assertEqual((game / 'redist/config/Espanol.lang').read_text(), 'translation')
            self.assertIn(b'LimitFPS="false"', personal.read_bytes())
            personal.write_bytes(personal.read_bytes().replace(b'<Graphics', b'<Screen Vsync="true"/><Graphics'))
            args.mode = 'restore'
            self.mod.run(args, package)
            self.assertEqual(self.mod.digest(game / 'redist/Penumbra.exe'), original)
            self.assertIn(b'LimitFPS="true"', personal.read_bytes())
            self.assertIn(b'<Screen Vsync="true"/>', personal.read_bytes())
            self.assertFalse((game / 'redist/config/Espanol.lang').exists())
            self.assertFalse((game / 'redist' / self.mod.BACKUP_MARKER).exists())

    def test_unknown_build_makes_no_backup(self):
        with tempfile.TemporaryDirectory() as temp:
            package, game, _, args = self.fixture(Path(temp))
            with self.assertRaisesRegex(ValueError, 'Unsupported'):
                self.mod.run(args, package)
            self.assertFalse((game / 'redist.before-penumbravr').exists())

    def test_failed_install_recovers_original(self):
        with tempfile.TemporaryDirectory() as temp:
            package, game, personal, args = self.fixture(Path(temp))
            original = self.mod.digest(game / 'redist/Penumbra.exe')
            before = personal.read_bytes()
            with patch.dict(self.mod.BUILDS, {'overture': (original,)}), patch.object(self.mod, 'copy_files', side_effect=OSError('copy failed')):
                with self.assertRaises(OSError):
                    self.mod.run(args, package)
            self.assertEqual(self.mod.digest(game / 'redist/Penumbra.exe'), original)
            self.assertEqual(personal.read_bytes(), before)

    def test_incomplete_backup_cannot_be_restored(self):
        with tempfile.TemporaryDirectory() as temp:
            package, game, _, args = self.fixture(Path(temp))
            (game / 'redist.before-penumbravr').mkdir()
            args.mode = 'restore'
            with self.assertRaisesRegex(ValueError, 'incomplete'):
                self.mod.run(args, package)

    def test_plan_does_not_touch_game_or_preferences(self):
        with tempfile.TemporaryDirectory() as temp:
            package, game, personal, args = self.fixture(Path(temp))
            original = self.mod.digest(game / 'redist/Penumbra.exe')
            before = personal.read_bytes()
            args.plan = True
            with patch.dict(self.mod.BUILDS, {'overture': (original,)}):
                self.mod.run(args, package)
            self.assertFalse((game / 'redist.before-penumbravr').exists())
            self.assertEqual(personal.read_bytes(), before)

    def test_restore_removes_new_translation_selection(self):
        with tempfile.TemporaryDirectory() as temp:
            package, game, personal, args = self.fixture(Path(temp))
            original = self.mod.digest(game / 'redist/Penumbra.exe')
            with patch.dict(self.mod.BUILDS, {'overture': (original,)}):
                self.mod.run(args, package)
            personal.write_bytes(personal.read_bytes().replace(b'English.lang', b'Espanol.lang'))
            args.mode = 'restore'
            self.mod.run(args, package)
            self.assertIn(b'English.lang', personal.read_bytes())

    def test_restore_checks_preferences_created_after_install(self):
        with tempfile.TemporaryDirectory() as temp:
            package, game, personal, args = self.fixture(Path(temp))
            personal.unlink()
            args.recommended_settings = False
            original = self.mod.digest(game / 'redist/Penumbra.exe')
            with patch.dict(self.mod.BUILDS, {'overture': (original,)}):
                self.mod.run(args, package)
            personal.write_bytes(b'<Game LanguageFile="Espanol.lang"/>')
            args.mode = 'restore'
            self.mod.run(args, package)
            self.assertIn(b'English.lang', personal.read_bytes())

    def test_black_plague_without_requiem_keeps_original_alut(self):
        with tempfile.TemporaryDirectory() as temp:
            package, game, _, args = self.fixture(Path(temp))
            (package / 'package-game.json').write_text(json.dumps({'game': 'black_plague'}))
            (package / 'installer-game-profile.json').write_text(json.dumps({'common': {}, 'games': {'black_plague': {}}}))
            (package / 'redist/alut.dll').write_bytes(b'proxy')
            (game / 'redist/alut.dll').write_bytes(b'original ALUT')
            lines = [self.mod.digest(p) + '  ' + p.relative_to(package).as_posix() + '\n'
                     for p in package.rglob('*') if p.is_file() and p.name != 'SHA256SUMS.txt']
            (package / 'SHA256SUMS.txt').write_text(''.join(lines))
            original = self.mod.digest(game / 'redist/Penumbra.exe')
            args.documents = None
            with patch.dict(self.mod.BUILDS, {'black_plague': (original,)}), patch.object(self.mod, 'ALUT', self.mod.digest(game / 'redist/alut.dll')):
                self.mod.run(args, package)
            self.assertEqual((game / 'redist/PenumbraVR_alut_original.dll').read_bytes(), b'original ALUT')
            self.assertFalse((game / 'redist/expansion01').exists())

    def test_symlink_game_tree_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            package, game, _, args = self.fixture(Path(temp))
            target = Path(temp) / 'elsewhere'
            target.write_bytes(b'outside')
            try:
                (game / 'redist/link').symlink_to(target)
            except OSError:
                self.skipTest('Host does not permit symlink creation')
            with self.assertRaisesRegex(ValueError, 'symlink'):
                self.mod.run(args, package)

    def test_corrupted_backup_is_not_marked_complete(self):
        with tempfile.TemporaryDirectory() as temp:
            package, game, _, args = self.fixture(Path(temp))
            original = self.mod.digest(game / 'redist/Penumbra.exe')
            copy = shutil.copytree
            def corrupt(source, destination, *positional, **keywords):
                result = copy(source, destination, *positional, **keywords)
                if Path(destination).name == 'redist.before-penumbravr':
                    (Path(destination) / 'Penumbra.exe').write_bytes(b'damaged backup')
                return result
            with patch.dict(self.mod.BUILDS, {'overture': (original,)}), patch.object(self.mod.shutil, 'copytree', side_effect=corrupt):
                with self.assertRaisesRegex(ValueError, 'Backup'):
                    self.mod.run(args, package)
            self.assertEqual(self.mod.digest(game / 'redist/Penumbra.exe'), original)
            self.assertFalse((game / 'redist.before-penumbravr' / self.mod.BACKUP_MARKER).exists())

    def test_backup_tampering_blocks_restore(self):
        with tempfile.TemporaryDirectory() as temp:
            package, game, _, args = self.fixture(Path(temp))
            original = self.mod.digest(game / 'redist/Penumbra.exe')
            with patch.dict(self.mod.BUILDS, {'overture': (original,)}):
                self.mod.run(args, package)
            (game / 'redist.before-penumbravr/Penumbra.exe').write_bytes(b'corrupted')
            args.mode = 'restore'
            with self.assertRaisesRegex(ValueError, 'Backup'):
                self.mod.run(args, package)

    def test_interrupted_restore_resumes_after_first_directory_rename(self):
        with tempfile.TemporaryDirectory() as temp:
            package, game, _, args = self.fixture(Path(temp))
            original = self.mod.digest(game / 'redist/Penumbra.exe')
            with patch.dict(self.mod.BUILDS, {'overture': (original,)}):
                self.mod.run(args, package)
            (game / 'redist').rename(game / 'redist.penumbravr-test')
            args.mode = 'restore'
            self.mod.run(args, package)
            self.assertEqual(self.mod.digest(game / 'redist/Penumbra.exe'), original)

    def test_unlisted_cache_payload_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            package, _, _, _ = self.fixture(Path(temp))
            path = package / 'redist/__pycache__/unlisted.dll'
            path.parent.mkdir()
            path.write_bytes(b'unlisted')
            with self.assertRaises(ValueError):
                self.mod.verify_package(package)

    def test_leftover_temp_file_does_not_block_restore(self):
        with tempfile.TemporaryDirectory() as temp:
            package, game, personal, args = self.fixture(Path(temp))
            original = self.mod.digest(game / 'redist/Penumbra.exe')
            with patch.dict(self.mod.BUILDS, {'overture': (original,)}):
                self.mod.run(args, package)
            leftover = personal.with_name(personal.name + '.penumbravr-tmp')
            leftover.write_bytes(b'interrupted temporary write')
            args.mode = 'restore'
            self.mod.run(args, package)
            self.assertIn(b'LimitFPS="true"', personal.read_bytes())
            self.assertEqual(leftover.read_bytes(), b'interrupted temporary write')

    def test_laa_matches_exact_transformed_hash(self):
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / 'game.exe'
            data = bytearray(256)
            data[:2] = b'MZ'
            data[60:64] = (128).to_bytes(4, 'little')
            data[128:134] = b'PE\0\0L\x01'
            target.write_bytes(data)
            data[150] |= 0x20
            expected = self.mod.hashlib.sha256(data).hexdigest()
            self.mod.apply_laa(target, expected)
            self.assertEqual(target.read_bytes(), data)

    def test_optional_textures_and_local_neural_model(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            package, game, _, args = self.fixture(root)
            pe = bytearray(256)
            pe[:2] = b'MZ'
            pe[60:64] = (128).to_bytes(4, 'little')
            pe[128:134] = b'PE\0\0d\x86'
            neural = package / 'optional/neural'
            neural.mkdir()
            for name in ('PenumbraNeuralHost.exe', 'nvngx.dll_nr_bridge.dll'):
                (neural / name).write_bytes(pe)
            model = root / 'nvngx_dlssnr.dll'
            model.write_bytes(pe)
            (package / 'optional/textures').mkdir()
            (package / 'optional/textures/texture.jpg').write_bytes(b'optional texture')
            (package / 'texture-selection.json').write_text(json.dumps({'Files': [{'Path': 'texture.jpg'}]}))
            (package / 'SHA256SUMS.txt').write_text(''.join(
                self.mod.digest(p) + '  ' + p.relative_to(package).as_posix() + '\n'
                for p in package.rglob('*') if p.is_file() and p.name != 'SHA256SUMS.txt'))
            args.neural_model, args.wine_redist, args.textures = str(model), 'Z:\\games\\Overture\\redist', True
            original = self.mod.digest(game / 'redist/Penumbra.exe')
            with patch.dict(self.mod.BUILDS, {'overture': (original,)}):
                self.mod.run(args, package)
            self.assertEqual((game / 'redist/texture.jpg').read_bytes(), b'optional texture')
            self.assertEqual((game / 'redist/PenumbraVR-Neural/NeuralRuntime/nvngx_dlssnr.dll').read_bytes(), pe)
            ini = (game / 'redist/NeuralPost.ini').read_bytes().decode('utf-16')
            self.assertIn('HostPath=Z:\\games\\Overture\\redist\\PenumbraVR-Neural\\PenumbraNeuralHost.exe', ini)
            self.assertIn('RuntimeDirectory=Z:\\games\\Overture\\redist\\PenumbraVR-Neural\\NeuralRuntime', ini)


if __name__ == '__main__':
    unittest.main()
