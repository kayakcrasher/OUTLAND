#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import tempfile, unittest, zipfile
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('stage',ROOT/'tools/stage_runtime_assets.py')
stage=importlib.util.module_from_spec(spec);spec.loader.exec_module(stage)
package_spec=importlib.util.spec_from_file_location('package',ROOT/'tools/package_android.py')
package=importlib.util.module_from_spec(package_spec);package_spec.loader.exec_module(package)
class RuntimePackaging(unittest.TestCase):
    def test_real_assets_are_included_without_source_archives(self):
        files=set(stage.runtime_files())
        for path in ('assets/verda/characters/character_manifest.tsv','assets/verda/vehicles/vehicle_manifest.tsv','assets/verda/survival/gameplay/items.tsv','maps/verda_loot_defaults.map'):
            self.assertIn(Path(path),files)
        self.assertGreater(len(files),1000)
        self.assertFalse(any('source' in p.parts or 'staging' in p.parts for p in files))
        self.assertFalse(any(p.suffix in ('.fbx','.blend','.zip','.rar') for p in files))
    def test_deterministic_manifest_and_updates_preserve_separate_saves(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);(root/'assets').mkdir();(root/'maps').mkdir()
            (root/'assets/model.glb').write_bytes(b'fixture');(root/'maps/world.map').write_text('world')
            (root/'saves').mkdir();(root/'saves/player').write_text('keep')
            first=stage.stage(root/'runtime',root)
            second=stage.stage(root/'runtime',root)
            self.assertEqual(first,second)
            self.assertEqual((root/'saves/player').read_text(),'keep')
            lines=(root/'runtime/runtime.manifest').read_text().splitlines()
            self.assertEqual(lines[1:],['assets/model.glb','maps/world.map'])
            (root/'assets/model.glb').write_bytes(b'changed')
            self.assertNotEqual(first['version'],stage.stage(root/'runtime',root)['version'])
    def test_staging_cannot_delete_source_assets(self):
        with self.assertRaises(ValueError): stage.stage(ROOT/'assets/runtime')
    def test_apk_preserves_unicode_asset_names(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);runtime=root/'runtime';(runtime/'assets').mkdir(parents=True)
            name='Radiation Workers – Body.glb'
            (runtime/'assets'/name).write_bytes(b'model')
            with zipfile.ZipFile(root/'resources.apk','w') as resources:resources.writestr('AndroidManifest.xml',b'manifest')
            (root/'native.so').write_bytes(b'library');(root/'classes.dex').write_bytes(b'dex')
            package.package(root/'resources.apk',runtime,root/'native.so',root/'classes.dex',root/'unsigned.apk')
            with zipfile.ZipFile(root/'unsigned.apk') as apk:
                entry=apk.getinfo('assets/assets/'+name)
                self.assertTrue(entry.flag_bits & 0x800)
                self.assertEqual(apk.read(entry),b'model')
                self.assertEqual(apk.getinfo('lib/arm64-v8a/liboutland.so').compress_type,zipfile.ZIP_STORED)
if __name__=='__main__':unittest.main()
