"""Build reproducible CC0 rusty steel and HDRI materials for UE4.27."""
import hashlib
import json
import os
import unreal

project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
base = "/Game/FreeAssets/PolyHaven/Realism"
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.MaterialEditingLibrary
report_path = os.path.join(project, "Saved", "RealismImport.json")
with open(report_path, "w", encoding="utf-8") as stream:
    json.dump({"state": "running"}, stream)


def import_maps(asset, prefix):
    folder = os.path.join(project, "Assets", "Source", "PolyHaven", asset)
    with open(os.path.join(folder, "manifest.json"), encoding="utf-8-sig") as stream:
        manifest = json.load(stream)
    result = {}
    for entry in manifest["files"]:
        source = os.path.join(folder, entry["file"])
        with open(source, "rb") as stream:
            if hashlib.sha256(stream.read()).hexdigest().lower() != entry["sha256"].lower():
                raise RuntimeError("Checksum failed: " + source)
        name = "T_" + prefix + "_" + entry["map"]
        path = base + "/Textures/" + name
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            task = unreal.AssetImportTask()
            task.filename = source
            task.destination_path = base + "/Textures"
            task.destination_name = name
            task.automated = True
            task.save = True
            tools.import_asset_tasks([task])
        tex = unreal.EditorAssetLibrary.load_asset(path)
        if not tex:
            raise RuntimeError("Texture import failed: " + path)
        tex.set_editor_property("max_texture_size", 512 if entry["map"] == "hdri" else 1024)
        tex.set_editor_property("srgb", entry["map"] == "Diffuse")
        if entry["map"] == "nor_dx":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        elif entry["map"] == "arm":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        unreal.EditorAssetLibrary.save_loaded_asset(tex)
        result[entry["map"]] = tex
    return result


def connect(source, output, destination, pin):
    if not lib.connect_material_expressions(source, output, destination, pin):
        raise RuntimeError("Material connection failed: {} -> {}.{}".format(source.get_class().get_name(), destination.get_class().get_name(), pin))
    return True


def new_material(name):
    path = base + "/Materials/" + name
    mat = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name, base + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    return mat


maps = import_maps("rusty_metal_04", "RustySteel")
mat = new_material("M_RustySteel")
uv = lib.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, 0)
tile = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 200)
tile.set_editor_property("parameter_name", "Tiling")
tile.set_editor_property("default_value", 1.0)
coords = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -700, 0)
connect(uv, "", coords, "A")
connect(tile, "", coords, "B")
for index, key in enumerate(("Diffuse", "nor_dx", "arm")):
    sample = lib.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, index * 200)
    sample.set_editor_property("texture", maps[key])
    connect(coords, "", sample, "UVs")
    if key == "Diffuse":
        tint = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, -200)
        tint.set_editor_property("parameter_name", "Color")
        tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
        mult = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -100, 0)
        connect(sample, "RGB", mult, "A")
        connect(tint, "", mult, "B")
        lib.connect_material_property(mult, "", unreal.MaterialProperty.MP_BASE_COLOR)
    elif key == "nor_dx":
        sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        lib.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_NORMAL)
    else:
        sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        for channel, prop in (("R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION), ("G", unreal.MaterialProperty.MP_ROUGHNESS), ("B", unreal.MaterialProperty.MP_METALLIC)):
            lib.connect_material_property(sample, channel, prop)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat)

cube = import_maps("industrial_sunset_02", "IndustrialSky")["hdri"]
if not isinstance(cube, unreal.TextureCube):
    raise RuntimeError("HDRI did not import as a cubemap")
sky = new_material("M_IndustrialSky")
sky.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
sky.set_editor_property("two_sided", True)
direction = lib.create_material_expression(sky, unreal.MaterialExpressionCameraVectorWS, -900, 0)
reverse = lib.create_material_expression(sky, unreal.MaterialExpressionMultiply, -700, 0)
reverse.set_editor_property("const_b", -1.0)
connect(direction, "", reverse, "A")
sample = lib.create_material_expression(sky, unreal.MaterialExpressionTextureSampleParameterCube, -500, 0)
sample.set_editor_property("texture", cube)
sample.set_editor_property("parameter_name", "SkyCube")
sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
connect(reverse, "", sample, "UVs")
gain = lib.create_material_expression(sky, unreal.MaterialExpressionMultiply, -200, 0)
gain.set_editor_property("const_b", 0.35)
connect(sample, "RGB", gain, "A")
lib.connect_material_property(gain, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(sky)
unreal.EditorAssetLibrary.save_loaded_asset(sky)

# Light fixtures have visible bulbs even on the mobile renderer without bloom.
fixture = new_material("M_FixtureGlow")
fixture.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
colour = lib.create_material_expression(fixture, unreal.MaterialExpressionConstant3Vector, -200, 0)
colour.set_editor_property("constant", unreal.LinearColor(1.8, 1.65, 1.3, 1))
lib.connect_material_property(colour, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(fixture)
unreal.EditorAssetLibrary.save_loaded_asset(fixture)

# Small translucent surface overlays, rather than deferred decals unsupported
# by some mobile paths. The silhouette is original math; grain comes from CC0.
grain = unreal.EditorAssetLibrary.load_asset("/Game/FreeAssets/PolyHaven/Warehouse/Textures/T_SandPlaster_diff")
if not grain:
    raise RuntimeError("Missing acquired plaster texture for surface grain")
for name, colour, opacity, roughness in (
        ("M_OilWear", (0.035, 0.026, 0.018), 0.65, 0.32),
        ("M_DustWear", (0.16, 0.11, 0.07), 0.48, 0.96),
        ("M_CrackWear", (0.025, 0.02, 0.017), 0.8, 0.9)):
    wear = new_material(name)
    wear.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    wear.set_editor_property("two_sided", True)
    uv = lib.create_material_expression(wear, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    offset = lib.create_material_expression(wear, unreal.MaterialExpressionSubtract, -700, 0)
    center = lib.create_material_expression(wear, unreal.MaterialExpressionConstant2Vector, -900, 200)
    center.set_editor_property("r", 0.5)
    center.set_editor_property("g", 0.5)
    connect(uv, "", offset, "A")
    connect(center, "", offset, "B")
    radius = lib.create_material_expression(wear, unreal.MaterialExpressionDotProduct, -500, 0)
    connect(offset, "", radius, "A")
    connect(offset, "", radius, "B")
    scale = lib.create_material_expression(wear, unreal.MaterialExpressionMultiply, -350, 0)
    scale.set_editor_property("const_b", 5.0)
    connect(radius, "", scale, "A")
    invert = lib.create_material_expression(wear, unreal.MaterialExpressionOneMinus, -200, 0)
    connect(scale, "", invert, "")
    clamp = lib.create_material_expression(wear, unreal.MaterialExpressionClamp, 0, 0)
    if not connect(invert, "", clamp, ""):
        raise RuntimeError("Could not connect wear material clamp input")
    texture = lib.create_material_expression(wear, unreal.MaterialExpressionTextureSample, -350, 300)
    texture.set_editor_property("texture", grain)
    alpha = lib.create_material_expression(wear, unreal.MaterialExpressionMultiply, 150, 0)
    connect(clamp, "", alpha, "A")
    connect(texture, "R", alpha, "B")
    strength = lib.create_material_expression(wear, unreal.MaterialExpressionMultiply, 300, 0)
    strength.set_editor_property("const_b", opacity)
    connect(alpha, "", strength, "A")
    lib.connect_material_property(strength, "", unreal.MaterialProperty.MP_OPACITY)
    base_colour = lib.create_material_expression(wear, unreal.MaterialExpressionConstant3Vector, 0, 300)
    base_colour.set_editor_property("constant", unreal.LinearColor(*colour, 1))
    lib.connect_material_property(base_colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = lib.create_material_expression(wear, unreal.MaterialExpressionConstant, 200, 300)
    rough.set_editor_property("r", roughness)
    lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(wear)
    unreal.EditorAssetLibrary.save_loaded_asset(wear)
with open(report_path, "w", encoding="utf-8") as stream:
    json.dump({"state": "complete", "steel": mat.get_path_name(), "sky": sky.get_path_name(), "cube": cube.get_path_name(), "fixture": fixture.get_path_name(), "hdri_face_limit": 512}, stream, indent=2)
unreal.log("TPSDuel: rusty steel and real industrial HDRI imported")
