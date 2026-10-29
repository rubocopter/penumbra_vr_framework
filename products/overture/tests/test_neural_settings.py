"""Host integration checks; no game, renderer, or headset is started.

Compile the real product settings and native setting widgets with small host
stubs. This exercises persistence and mouse edits without linking the game.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

PRODUCT = Path(__file__).resolve().parents[1]
GAME = PRODUCT / "PenumbraOverture"


class NeuralIntegration(unittest.TestCase):
    def test_compiled_product_persistence_and_native_widgets(self):
        menu = (GAME / "MainMenu.cpp").read_text()
        header = (GAME / "MainMenu.h").read_text()
        settings = (GAME / "VRSettings.cpp").read_text()
        fixture = Path(__file__).parent / "NeuralSettingsHost"
        # Keep the production implementations intact; replace engine includes
        # and allocation/font/config boundaries with a small host environment.
        settings = re.sub(r'^#include "[^"]+"\s*$', '', settings, flags=re.M)
        states = re.search(r'enum eMainMenuState\s*\{.*?\};', header, re.S).group()
        back_states = re.search(r'static eMainMenuState gvMenuBackStates\[\].*?\};', menu, re.S).group()
        widgets = menu[menu.index("enum eVRMenuSetting"):menu.index("class cMainMenuWidget_RecenterButton")]
        text_click = menu[menu.index("void cMainMenuWidget_Text::OnMouseDown"):menu.index("void cMainMenuWidget_Text::OnMouseOver")]
        state_switch = menu[menu.index("void cMainMenu::SetState("):menu.index("void cMainMenu::SetInputToAction(")]
        page = menu.split("// Neural Settings", 1)[1].split("// Options Controls", 1)[0]
        main_page = menu.split("// VR Settings\n", 1)[1].split("RefreshVRSettingAvailability(mpInit);", 1)[0]
        source = '\n'.join([
            '#include "VRSettings.h"',
            '#include "config_stub.hpp"', settings, states, back_states,
            '#include "widgets_stub.hpp"', text_click, state_switch, widgets,
            'static void RefreshVRSettingAvailability(cInit*) {}',
            'static void CreateMainPage(cInit* mpInit) { cVector3f vTextStart(220, 230, 40), vPos;', main_page, '}',
            'static void CreateNeuralPage(cInit* mpInit) {', page, '}',
            '#include "checks.hpp"',
        ])
        vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
        self.assertTrue(vswhere.exists(), "Visual C++ Build Tools are required for these product host tests")
        install = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
            'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        vcvars = Path(install) / "VC/Auxiliary/Build/vcvarsall.bat"
        # Isolated test compilation only: never invokes the product build.
        output = PRODUCT / "build/tests"
        output.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="neural-host-", dir=output) as temp:
            directory = Path(temp)
            cpp = directory / "integration.cpp"
            cpp.write_text(source)
            args = ['cl', '/nologo', '/EHsc', '/std:c++14', '/W3', '/WX',
                '/I' + str(GAME), '/I' + str(fixture), str(cpp), '/Fe:integration.exe']
            build = directory / "compile.cmd"
            build.write_text('@echo off\ncall "' + str(vcvars) + '" x86 >nul\n' + subprocess.list2cmdline(args) + '\n')
            result = subprocess.run(['cmd', '/d', '/c', str(build)], cwd=directory, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(directory / "integration.exe")], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            print(result.stdout.strip())

    def test_product_settings_api(self):
        header = (GAME / "VRSettings.h").read_text()
        self.assertIn("GetNeuralSettings() const", header)
        self.assertIn("SetNeuralSettings(", header)

    def test_page_navigation_localization_and_live_apply(self):
        menu = (GAME / "MainMenu.cpp").read_text()
        header = (GAME / "MainMenu.h").read_text()
        self.assertIn("eMainMenuState_OptionsVRNeural", header)
        self.assertRegex(menu, r"eMainMenuState_OptionsVRSettings,\s*//eMainMenuState_OptionsVRNeural")
        self.assertIn("neural_post::StatusText()", menu)
        init = (GAME / "Init.cpp").read_text()
        apply = init.split("void cInit::ApplyVRSettings(bool abSave)", 1)[1].split("void cInit::ResetGame", 1)[0]
        self.assertIn("neural_post::Configure(mVRSettings.GetNeuralSettings())", apply)
        self.assertIn("ApplyVRSettings(false)", init)
        page = menu.split("// Neural Settings", 1)[1].split("// Options Controls", 1)[0]
        self.assertIn("eMainMenuState_OptionsVRSettings", page)
        rows = re.findall(r"eVRMenuSetting_Neural\w+", page)
        self.assertEqual(len(set(rows)), 9)
        # Verify every new visible label exists exactly once in both product languages.
        keys = set(re.findall(r'return "(VRNeural\w+)"', menu)) | set(re.findall(r'kTranslate\("MainMenu", "(VRNeural\w+)"\)', menu))
        for language in ("English.lang", "Espanol.lang"):
            root = ET.parse(PRODUCT / "data/config" / language).getroot()
            entries = [entry.attrib.get("Name") for category in root if category.attrib.get("Name") == "MainMenu" for entry in category]
            for key in keys:
                self.assertEqual(entries.count(key), 1, (language, key))


if __name__ == "__main__":
    unittest.main()
