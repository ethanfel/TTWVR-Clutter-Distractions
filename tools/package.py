# SPDX-License-Identifier: GPL-3.0-only
"""Create independent, Data-root mod-manager archives."""
from pathlib import Path
import sys
from zipfile import ZIP_DEFLATED, ZipFile

def package(name, version, files):
    output = Path("build", f"{name}-{version}.zip")
    temp = output.with_suffix(".zip.tmp")
    with ZipFile(temp, "w", ZIP_DEFLATED) as archive:
        archive.write("LICENSE", f"docs/{name}/LICENSE.txt")
        for source, member in files:
            archive.write(source, member)
    temp.replace(output)
    print(output)

def plugin(name):
    dll = name + ".dll"
    return (Path("build", dll), "NVSE/Plugins/" + dll)

if __name__ == "__main__":
    version = sys.argv[1]
    package("TTWVRClutterAudio", version, [plugin("TTWVRClutterAudio")])
    # Existing saves and holster bindings refer to the original ESP and form IDs.
    esp = "TTWVRClutterDistractions.esp"
    ini = "TTWVRClutterDistractions.ini"
    meshes = [Path("meshes/TTWVRClutter") / f for f in ("Held.nif", "Portable.nif")]
    package("TTWVRBearTraps", version, [
        plugin("TTWVRBearTraps"), (Path("build", esp), esp),
        (Path("config", ini), "NVSE/Plugins/" + ini),
        *((Path("assets") / mesh, mesh.as_posix()) for mesh in meshes),
    ])
