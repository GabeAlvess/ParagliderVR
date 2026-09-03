using Mutagen.Bethesda;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Plugins.Records;
using Mutagen.Bethesda.Skyrim;
using Noggog;

const uint paragliderArmorLocalFormId = 0x000800;
const uint paragliderArmorAddonLocalFormId = 0x000801;
const uint paragliderCarrierLocalFormId = 0x000802;
const uint paragliderRecipeLocalFormId = 0x000803;
const uint templateArmorLocalFormId = 0x000801;
const uint templateArmorAddonLocalFormId = 0x000800;
const uint slot42Mask = 1u << 12;
const uint firewoodLocalFormId = 0x0006F993;
const uint leatherLocalFormId = 0x000DB5D2;
const uint smithingForgeKeywordLocalFormId = 0x00088105;
const string pluginName = "ParagliderVR.esp";

var outputPath = args.Length > 0
    ? Path.GetFullPath(args[0])
    : Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "Assets", pluginName));
var templateArgument = args.Length > 1
    ? args[1]
    : Environment.GetEnvironmentVariable("PARAGLIDER_TEMPLATE_ESP");
if (string.IsNullOrWhiteSpace(templateArgument))
{
    throw new ArgumentException(
        "Provide a template plugin as the second argument or set PARAGLIDER_TEMPLATE_ESP.");
}
var templatePath = RequireExistingFile(templateArgument);

Directory.CreateDirectory(Path.GetDirectoryName(outputPath)!);

var templateKey = ModKey.FromFileName(Path.GetFileName(templatePath));
var template = SkyrimMod.CreateFromBinary(
    new ModPath(templateKey, templatePath),
    SkyrimRelease.SkyrimSE);
var sourceArmor = template.Armors.Single(record => record.FormKey.ID == templateArmorLocalFormId);
var sourceArmorAddon = template.ArmorAddons.Single(record => record.FormKey.ID == templateArmorAddonLocalFormId);

var modKey = ModKey.FromFileName(pluginName);
var mod = new SkyrimMod(modKey, SkyrimRelease.SkyrimSE)
{
    IsSmallMaster = true
};
mod.ModHeader.Author = "Alves";
mod.ModHeader.Description = "Craftable physical paraglider carrier for ParagliderVR.";

var armorAddon = mod.ArmorAddons.DuplicateInAsNewRecord(
    sourceArmorAddon,
    new FormKey(modKey, paragliderArmorAddonLocalFormId));
armorAddon.EditorID = "ParagliderVRArmorAddon";
armorAddon.BodyTemplate ??= new BodyTemplate();
armorAddon.BodyTemplate.FirstPersonFlags = (BipedObjectFlag)slot42Mask;
armorAddon.BodyTemplate.ArmorType = ArmorType.Clothing;

var armor = mod.Armors.DuplicateInAsNewRecord(
    sourceArmor,
    new FormKey(modKey, paragliderArmorLocalFormId));
armor.EditorID = "ParagliderVRArmor";
armor.Name = "Paraglider";
armor.Description = "A light wooden and leather paraglider frame.";
armor.ObjectBounds = new ObjectBounds
{
    First = new P3Int16(-52, -28, -2),
    Second = new P3Int16(30, 28, 24)
};
armor.WorldModel = new GenderedItem<ArmorModel?>(CreateArmorModel(), CreateArmorModel());
armor.BodyTemplate ??= new BodyTemplate();
armor.BodyTemplate.FirstPersonFlags = (BipedObjectFlag)slot42Mask;
armor.BodyTemplate.ArmorType = ArmorType.Clothing;
armor.ArmorRating = 0;
armor.Value = 150;
armor.Weight = 1.0f;
armor.Armature.Clear();
armor.Armature.Add(new FormLink<IArmorAddonGetter>(armorAddon.FormKey));

var carrier = new Mutagen.Bethesda.Skyrim.Activator(
    new FormKey(modKey, paragliderCarrierLocalFormId),
    SkyrimRelease.SkyrimSE)
{
    EditorID = "ParagliderVRPhysicalCarrier",
    Name = "Paraglider",
    Model = new Model
    {
        File = @"Paraglider\GliderPhysical.nif"
    }
};
mod.Activators.Add(carrier);

var skyrimKey = ModKey.FromFileName("Skyrim.esm");
var recipe = new ConstructibleObject(
    new FormKey(modKey, paragliderRecipeLocalFormId),
    SkyrimRelease.SkyrimSE)
{
    EditorID = "ParagliderVRForgeRecipe",
    CreatedObject = new FormLinkNullable<IConstructibleGetter>(armor.FormKey),
    CreatedObjectCount = 1,
    WorkbenchKeyword = new FormLinkNullable<IKeywordGetter>(
        new FormKey(skyrimKey, smithingForgeKeywordLocalFormId)),
    Items = new ExtendedList<ContainerEntry>()
};
recipe.Items.Add(CreateIngredient(skyrimKey, firewoodLocalFormId, 2));
recipe.Items.Add(CreateIngredient(skyrimKey, leatherLocalFormId, 5));
mod.ConstructibleObjects.Add(recipe);

mod.WriteToBinary(outputPath);

var generated = SkyrimMod.CreateFromBinary(new ModPath(modKey, outputPath), SkyrimRelease.SkyrimSE);
var verifiedArmor = generated.Armors.Single(record => record.FormKey.ID == paragliderArmorLocalFormId);
var verifiedAddon = generated.ArmorAddons.Single(record => record.FormKey.ID == paragliderArmorAddonLocalFormId);
var verifiedCarrier = generated.Activators.Single(record => record.FormKey.ID == paragliderCarrierLocalFormId);
var verifiedRecipe = generated.ConstructibleObjects.Single(record => record.FormKey.ID == paragliderRecipeLocalFormId);
if ((uint)(verifiedArmor.BodyTemplate?.FirstPersonFlags ?? 0) != slot42Mask ||
    (uint)(verifiedAddon.BodyTemplate?.FirstPersonFlags ?? 0) != slot42Mask ||
    verifiedArmor.Weight != 1.0f || verifiedArmor.Armature.Count != 1 ||
    verifiedCarrier.Model?.File.ToString() != @"Meshes\Paraglider\GliderPhysical.nif" ||
    verifiedRecipe.CreatedObject.FormKey != verifiedArmor.FormKey || verifiedRecipe.Items?.Count != 2)
{
    throw new InvalidDataException("Generated ParagliderVR plugin failed verification.");
}

Console.WriteLine($"Generated {outputPath}");
Console.WriteLine($"Armor local FormID: 0x{paragliderArmorLocalFormId:X6}");
Console.WriteLine($"Armor addon local FormID: 0x{paragliderArmorAddonLocalFormId:X6}");
Console.WriteLine($"Physical carrier local FormID: 0x{paragliderCarrierLocalFormId:X6}");
Console.WriteLine($"Forge recipe local FormID: 0x{paragliderRecipeLocalFormId:X6}");

static ArmorModel CreateArmorModel()
{
    return new ArmorModel
    {
        Model = new Model
        {
            File = @"Paraglider\GliderPhysical.nif"
        }
    };
}

static ContainerEntry CreateIngredient(ModKey skyrimKey, uint localFormId, int count)
{
    return new ContainerEntry
    {
        Item = new ContainerItem
        {
            Item = new FormLink<IItemGetter>(new FormKey(skyrimKey, localFormId)),
            Count = count
        }
    };
}

static string RequireExistingFile(string path)
{
    var fullPath = Path.GetFullPath(path);
    if (!File.Exists(fullPath))
    {
        throw new FileNotFoundException("Required plugin template was not found.", fullPath);
    }
    return fullPath;
}
