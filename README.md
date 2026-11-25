# TTWVR Clutter Audio and Bear Traps

Two independent add-ons for Tale of Two Wastelands VR. Install either one or both.

| Add-on | What it does | Download |
| --- | --- | --- |
| **Clutter Audio** | Throw a can, bump loose clutter, or tap a held object with a weapon. Nearby enemies can hear the impact where it happens. | [Clutter Audio 0.3.0](https://github.com/ethanfel/TTWVR-Clutter-Distractions/releases/download/v0.3.0/TTWVRClutterAudio-0.3.0.zip) |
| **Bear Traps** | Carry a repairable trap in a holster, open it with your hands, and place it on the ground. | [Bear Traps 0.3.0](https://github.com/ethanfel/TTWVR-Clutter-Distractions/releases/download/v0.3.0/TTWVRBearTraps-0.3.0.zip) |

## Install

Requires TTWVR 1.1.7, Fallout New Vegas 1.4.0.525, xNVSE 6.3+ and JIP LN NVSE.

1. Close Fallout. Install the ZIPs you want with your mod manager and enable them in your VR profile. For a manual install, extract each ZIP into `Fallout New Vegas/Data`.
2. If you installed **Bear Traps**, enable **`TTWVRClutterDistractions.esp`**. Clutter Audio loads automatically through xNVSE.
3. Start Fallout and load your save.

**Upgrading from the combined mod or a test build?** Replace the older packages. Disable anything that still supplies `NVSE/Plugins/TTWVRClutterDistractions.dll`, including the old equip patch or bundled audio test room. Bear Traps keeps the original ESP and form IDs so existing traps and holster assignments carry over. Copy your customized `TTWVRClutterDistractions.ini` into the new Bear Traps mod if you changed its settings.

## Clutter Audio

Grab a loose object and release your grip during the swing to throw it. Open a still hand to drop it. You can also make noise by walking into loose clutter or striking an object in your hand.

## Bear Traps

1. Craft a **Portable Bear Trap** at a workbench with four scrap metal. Each trap weighs 10.
2. In **Pip-Boy → Weapons**, assign **Portable Bear Trap (Place)** to a holster. The tool draws from your trap stock in Misc.
3. Draw the tool. Grip the opposite jaw with your free hand and pull until the latch clicks. Release the free hand, then press the drawing-hand trigger to drop the trap. It arms when it lands. You can also place a closed trap and pull its jaws apart on the ground. Arming requires **Repair 55** by default.
4. To reach the floor, hold the right stick down for three seconds. From deep crouch, tap once for regular crouch and again to stand.
5. To disarm, hold both sides for a moment or activate the trap and choose **Disarm**. After it springs, activate the folded trap to pick it up.

A sprung trap holds an enemy for four seconds, interrupts firing, and injures a leg. The player is caught for one second. Each snap costs 10 condition points by default. Disarming costs nothing. One scrap metal restores 25 points at a workbench.

### Trap settings

Edit `Data/NVSE/Plugins/TTWVRClutterDistractions.ini` and restart Fallout.

| Setting | Default | Effect |
| --- | --- | --- |
| `MinimumRepair` | `55` | Repair skill required to arm a trap, from 0 to 100. |
| `HandheldArming` | `1` | Set to 0 to arm traps only on the ground. |
| `ConditionDegradationFactor` | `1.0` | Wear per snap. 0 disables wear, 0.5 costs 5 points, and 2 costs 20. The factor rounds to the nearest 0.5 up to 10. |
| `EyeHeightCm` | `55` | Deep crouch eye height, from 30 to 120 cm. Set to 0 to disable deep crouch. |

## Logs and source

Each add-on writes its own log in `Data/NVSE/Plugins` after loading a save. Look for `TTWVRClutterAudio.log` or `TTWVRBearTraps.log`.

Build both release ZIPs with `make package`. This runs the tests and writes the archives to `build/`. It requires GNU Make, Python 3, i686 MinGW-w64 with C++17, and native g++ with 32-bit multilib. Developers can build the optional ghoul hearing room with `make audio-stage`. To regenerate the folded trap meshes, run `tools/portable_mesh.py` with PyFFI, the extracted vanilla `BearTrap01.nif`, `TinCan01.nif`, and the `assets/meshes/TTWVRClutter` output directory.

The add-on code is licensed under [GPL-3.0-only](LICENSE). Fallout assets belong to their respective owners.
