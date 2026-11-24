# TTWVR Clutter Distractions

Throw a can, bump loose clutter, or strike a held object to make nearby enemies investigate the sound. A portable bear trap also draws attention when it snaps.

## Install

1. Download the [0.2.7 test build](https://github.com/ethanfel/TTWVR-Clutter-Distractions/releases/download/v0.2.7/TTWVRClutterDistractions-0.2.7.zip).
2. Install the ZIP with your mod manager. Enable the mod and **TTWVRClutterDistractions.esp** in your VR profile. For manual installation, extract the ZIP into `Fallout New Vegas/Data`.
3. Restart Fallout and load your save.

Requires TTWVR 1.1.7, Fallout New Vegas 1.4.0.525, xNVSE 6.3+ and JIP LN NVSE.

## Portable bear trap

- Craft **Portable Bear Trap** at a workbench with four scrap metal. It weighs **10**.
- Once you carry a trap, open **Pip-Boy → Weapons** and assign **Portable Bear Trap (Place)** to a holster slot. This entry uses the traps in Misc. It adds no weight and keeps its slot when stock runs out.
- Draw it using the holster’s grip. Lower the trap toward a surface and press the **trigger on the drawing hand** to release one closed trap. It falls from your hand and settles on the surface below. Releasing the grip before placement uses the usual TTWVR holster behavior. You can also drop a trap directly from Misc.
- Tap the right stick down to crouch. Hold it down for **3 seconds** to reach the floor with your hands. Your knees bend and your feet stay under your body. From deep crouch, one tap returns to regular crouch and a second tap stands up.
- With **Repair 55**, grip both sides of the closed trap and pull your hands apart until you hear the latch click. Hold both sides of an armed trap for a moment to disarm it, or activate it and choose **Disarm**. Once you step clear, returning to the trap can spring it. The game honors the Light Step perk.
- When the jaws snap, the impact sounds at the trap. An enemy stays caught for four seconds and takes a crippled leg. Super mutants recoil on the caught side, including during attacks if kNVSE is installed. After release, the limp depends on the creature and stance. The player’s catch lasts one second.
- Activate a closed trap to pick it back up. Each snap costs **10 condition points**, so a fresh trap lasts ten snaps. Disarming it costs nothing. At a workbench, one scrap metal restores 25 points, up to 100%. You can recover and repair a broken trap.

Edit the included `Data/NVSE/Plugins/TTWVRClutterDistractions.ini` and restart Fallout. Set `MinimumRepair` from 0 to 100. At 0, any character can arm a trap. `EyeHeightCm` sets the deeper crouch height (30–120 cm). Set it to 0 to keep only regular crouching.

```ini
[BearTrap]
MinimumRepair=55

[DeepCrouch]
EyeHeightCm=55
```

## Build

Run `make package` with GNU Make, Python 3, an i686 MinGW-w64 C++17 compiler and native g++ with 32-bit multilib support. The portable meshes use a fixed closed pose derived from Fallout’s bear trap. The original game supplies their textures and the animated armed model. To regenerate the portable meshes, run `tools/portable_mesh.py` with PyFFI, the extracted vanilla `BearTrap01.nif`, `TinCan01.nif`, and `assets/meshes/TTWVRClutter` as arguments. The tested package is written to `build/`. In-game diagnostics go to `Data/NVSE/Plugins/TTWVRClutterDistractions.log`.

The addon code uses the [GNU General Public License v3.0 only](LICENSE). Fallout assets belong to their respective owners.
