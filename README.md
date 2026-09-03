# ParagliderVR

Native SKSE VR physical paraglider for Skyrim VR.

Current release: `0.100`. Future releases increment sequentially as `0.101`,
`0.102`, and so on.

## Requirements

- Skyrim VR
- SKSEVR
- HIGGS
- VRIK
- Open Animation Replacer

## Gameplay

- Craft the Paraglider at a forge with 2 firewood and 5 leather.
- The item prefers biped slot 42 and selects the nearest free slot at runtime.
- Equipping the item enables deployment without showing a model on the player.
- While airborne, raise both hands above the HMD and hold both grip buttons.
- The physical glider deploys into both HIGGS hands after alignment.
- During flight, either held hand may descend up to 10 Skyrim units below the HMD.
- Releasing one grip enables one-handed flight; releasing both retracts the glider.
- Retraction returns the physical glider to the inventory state without unequipping the item.

Flight continuously consumes stamina. Gameplay and visual tuning are available
in `Data/SKSE/Plugins/ParagliderVR.ini`.

The OAR package animates the lower body while VRIK retains control of the torso,
head, arms, and hands.

## Build

Clone with submodules, or initialize them after cloning:

```powershell
git submodule update --init --recursive
xmake f -m releasedbg -y
xmake -y
```

The complete mod is written to `install_output`.

## Credits

See [CREDITS.md](CREDITS.md) for project and asset attribution.
