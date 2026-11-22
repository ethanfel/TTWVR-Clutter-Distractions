# TTWVR Clutter Distractions

Throw a can, bump loose clutter, or strike a held object to make nearby enemies investigate the sound. A portable bear trap also draws attention when it snaps.

## Install

1. Download the [0.2.1 test build](https://github.com/ethanfel/TTWVR-Clutter-Distractions/releases/download/v0.2.1/TTWVRClutterDistractions-0.2.1.zip).
2. Install the ZIP with your mod manager. Enable the mod and **TTWVRClutterDistractions.esp** in your VR profile. For manual installation, extract the ZIP into `Fallout New Vegas/Data`.
3. Restart Fallout and load your save.

Requires TTWVR 1.1.7, Fallout New Vegas 1.4.0.525, xNVSE 6.3+ and JIP LN NVSE.

## Portable bear trap

- Craft **Portable Bear Trap** at a workbench with four scrap metal. It weighs **10**.
- Drop it from **Misc**. Grip the closed trap with either hand to move it. Release to place it on the surface below your hand.
- To reach the floor, hold the **right stick down for 3 seconds** as you crouch. Release the stick to stay low and use both hands. Tap it down again to stand.
- With **Repair 55**, grip both sides of the closed trap and pull your hands apart. Hold both sides of an armed trap for a moment to disarm it.
- Activate a closed trap to pick it back up. Each time it springs, it loses **25% condition**. At a workbench, one scrap metal restores 25%. You can recover and repair a broken trap.

Edit the included `Data/NVSE/Plugins/TTWVRClutterDistractions.ini` and restart Fallout. Set `MinimumRepair` from 0 to 100. At 0, any character can arm a trap. `EyeHeightCm` sets the deeper crouch height (30–120 cm). Set it to 0 to keep only regular crouching.

```ini
[BearTrap]
MinimumRepair=55

[DeepCrouch]
EyeHeightCm=55
```

## Build

Run `make package` with GNU Make, Python 3, an i686 MinGW-w64 C++17 compiler and native g++ with 32-bit multilib support. The tested package is written to `build/`. In-game diagnostics go to `Data/NVSE/Plugins/TTWVRClutterDistractions.log`.

[GNU General Public License v3.0 only](LICENSE).
