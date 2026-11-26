#!/usr/bin/env bash
# Experimental Proton/Wine file overlay for the allowlisted Windows Steam builds.
set -euo pipefail

usage() {
    echo "Usage: bash install-linux.sh install|restore '/path/to/Penumbra Black Plague'" >&2
    exit 2
}

[[ $# -eq 2 ]] || usage
mode=$1
[[ "$mode" == install || "$mode" == restore ]] || usage
overlay_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
game_root=$(cd -- "$2" && pwd -P)
redist="$game_root/redist"
backup="$game_root/redist.before-penumbravr"
displaced="$game_root/redist.penumbravr-test"
[[ -d "$redist" ]] || { echo "Missing game redist directory: $redist" >&2; exit 1; }

if [[ "$mode" == restore ]]; then
    [[ -d "$backup" ]] || { echo 'No original redist backup to restore.' >&2; exit 1; }
    [[ ! -e "$displaced" ]] || { echo "Restore destination already exists: $displaced" >&2; exit 1; }
    mv -- "$redist" "$displaced"
    mv -- "$backup" "$redist"
    echo "Original files restored. Test the game before deleting $displaced"
    exit 0
fi

for program in sha256sum cp; do
    command -v "$program" >/dev/null || { echo "Missing command: $program" >&2; exit 1; }
done
[[ ! -e "$backup" && ! -e "$displaced" ]] || {
    echo 'Existing backup or test directory: inspect previous installation first.' >&2
    exit 1
}
[[ -f "$redist/Penumbra.exe" && -f "$redist/alut.dll" ]] || {
    echo 'Black Plague Windows executable or original ALUT is missing.' >&2
    exit 1
}
hash_is() {
    [[ "$(sha256sum -- "$1" | cut -d ' ' -f 1)" == "$2" ]]
}
hash_is "$redist/Penumbra.exe" fd316f7586737a63eba989ece2271280fe6a98582a1319fe2151385a3df97bff ||
hash_is "$redist/Penumbra.exe" db086cc7a4c7b10864de0febbe2d71a3e4eff1ec8d067811a6a59edc1c617196 || {
    echo 'Unsupported or modified Black Plague executable.' >&2
    exit 1
}
hash_is "$redist/alut.dll" d81dea8e88e35c319f7f2d8aaeb14c63a4986131492d3df860d1f2c18b844590 || {
    echo 'Unsupported or already modified original alut.dll.' >&2
    exit 1
}
if [[ -f "$redist/Requiem.exe" ]]; then
    hash_is "$redist/Requiem.exe" b64232d751cee376e1384cfe5a4a81dbd7deddf03983cc11d0d0a34d5825eea2 ||
    hash_is "$redist/Requiem.exe" 577d1d7780872cd6c5b99b45759cdc48feea486a1ccbf319e8f6cf0eaed54e955 || {
        echo 'Unsupported or modified Requiem executable.' >&2
        exit 1
    }
    [[ -f "$redist/expansion01/config/English_exp.lang" ]] || {
        echo 'Requiem expansion data is missing.' >&2
        exit 1
    }
fi
for path in PenumbraVR_alut_original.dll PenumbraVR.BlackPlague.Probe.dll \
            PenumbraVR.Requiem.Probe.dll PenumbraVR_OpenALSoft.dll \
            openvr_api.dll vr assets/rework; do
    [[ ! -e "$redist/$path" ]] || {
        echo "Existing mod files at redist/$path: use an unmodified game copy." >&2
        exit 1
    }
done
if [[ -e "$redist/OpenAL32.dll" ]]; then
    hash_is "$redist/OpenAL32.dll" 606cf8b4c22c7ae00813585279ac045115907bc66d1483eb26355907021ea4b0 || {
        echo 'An unknown OpenAL32.dll is already installed.' >&2
        exit 1
    }
fi
(cd -- "$overlay_root" && sha256sum -c SHA256SUMS.txt)

# Copy the entire retail folder before making any changes. The original ALUT
# must remain available for the forwarded DLL exports in the VR bootstrap.
cp -a -- "$redist" "$backup"
cp -a -- "$backup/alut.dll" "$redist/PenumbraVR_alut_original.dll"
cp -a -- "$overlay_root/redist/." "$redist/"
echo 'VR overlay installed. Start SteamVR, then launch through Steam Proton.'
echo "To restore: bash '$overlay_root/install-linux.sh' restore '$game_root'"
