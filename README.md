# TTWVR Clutter Distractions

Toss a tin can down a corridor to draw nearby enemies to its landing spot. You can also knock loose objects with your body, hit something with an object in hand, or strike a held can with your weapon. A harder strike carries further. Hostile NPCs use Fallout's normal hearing and search behavior.

## Install

Requires a working TTWVR setup with Fallout: New Vegas 1.4.0.525, xNVSE 6.3+ and JIP LN NVSE.

1. Download [TTWVR Clutter Distractions 0.1.0](https://github.com/ethanfel/TTWVR-Clutter-Distractions/releases/download/v0.1.0/TTWVRClutterDistractions-0.1.0.zip).
2. Install the ZIP with your mod manager and enable it in your VR profile. For manual installation, copy the ZIP’s `NVSE` folder into `Fallout New Vegas/Data`.
3. Restart Fallout and load a save.

## Headset check

From cover near an unaware enemy, gently set down a can, then throw it past a doorway. Try another can held in one hand. Strike it with a drawn weapon, hold the weapon against it, separate it and tap again. Compare the two places the enemy investigates.

The addon logs each detection event it submits to `Data/NVSE/Plugins/TTWVRClutterDistractions.log`. The line includes the object, sound level and world position. `recorded=1` means the player's detection event contains that position. The enemy's response depends on distance, current alert state and AI package.

## Build

On Linux, run `make package` with GNU Make, Python 3, an i686 MinGW-w64 C++17 compiler and native g++ with 32-bit multilib support. This runs the tests and writes the installable ZIP to `build/`.

## License

[GNU General Public License v3.0 only](LICENSE).
