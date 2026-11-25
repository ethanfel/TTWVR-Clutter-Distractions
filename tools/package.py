# SPDX-License-Identifier: GPL-3.0-only
"""Package the plugin, configuration and both meshes in a mod-manager Data root."""
from pathlib import Path
import sys
from zipfile import ZIP_DEFLATED, ZipFile

output = Path(sys.argv[1])
name = "TTWVRClutterDistractions"
meshes = Path("meshes/TTWVRClutter")
files = {
    Path("build", name + ".esp"): name + ".esp",
    Path("build", name + ".dll"): f"NVSE/Plugins/{name}.dll",
    Path("config", name + ".ini"): f"NVSE/Plugins/{name}.ini",
}
for mesh in ("Portable.nif", "Held.nif"):
    files[Path("assets") / meshes / mesh] = (meshes / mesh).as_posix()
temporary = output.with_suffix(".zip.tmp")
with ZipFile(temporary, "w", ZIP_DEFLATED) as archive:
    for source, member in files.items():
        if member.endswith(".ini") and "--handheld-test" in sys.argv[2:]:
            config = source.read_text()
            assert config.count("HandheldArming=0") == 1
            archive.writestr(member, config.replace("HandheldArming=0", "HandheldArming=1"))
        else:
            archive.write(source, member)
temporary.replace(output)
print(output)
