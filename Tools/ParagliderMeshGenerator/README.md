# Physical Paraglider Mesh Generator

Copies the complete visual glider, adds one thin four-corner cloth panel plus three simple rectangular collision pieces
for each grip/structure side, and places `HIGGS:GrabL` / `HIGGS:GrabR` on the
two lower structural ropes.

```powershell
python Tools/ParagliderMeshGenerator/generate_physical_glider.py `
  --visual Assets/meshes/Paraglider/Glider.nif `
  --collision-template Tools/ParagliderMeshGenerator/collision_template.nif `
  --output Assets/meshes/Paraglider/GliderPhysical.nif `
  --preview-obj artifacts/glider_collision_preview.obj
```

The default rigid-body mass is `0.35`. Collision uses exactly seven lightweight pieces: one thin cloth panel, three boxes following each real lateral frame, including the matching frame on the hidden side.
