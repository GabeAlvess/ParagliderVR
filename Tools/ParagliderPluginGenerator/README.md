# Paraglider Plugin Generator

Generates the ESL-flagged `Assets/ParagliderVR.esp` containing:

- a craftable clothing item initially assigned to biped slot 42;
- an invisible armor addon whose slot mask is updated with the armor at runtime;
- a non-inventory physical activator used by HIGGS;
- a forge recipe requiring 2 firewood and 5 leather.

```powershell
dotnet run --project Tools/ParagliderPluginGenerator -- Assets/ParagliderVR.esp path\to\Template.esp
```

The second argument must be a plugin containing compatible armor and armor-addon
template records at local FormIDs `000801` and `000800`. Alternatively, set the
`PARAGLIDER_TEMPLATE_ESP` environment variable to that plugin path.
