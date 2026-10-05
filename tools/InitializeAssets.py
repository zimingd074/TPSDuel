"""Run inside UE4.27 Editor; never substitute external Python for the unreal module."""
import math
import os
import random
import struct
import wave
import unreal

project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
for folder in ("/Game/Maps", "/Game/Materials", "/Game/Audio"):
    unreal.EditorAssetLibrary.make_directory(folder)

material_path = "/Game/Materials/M_DuelColor"
if not unreal.EditorAssetLibrary.does_asset_exist(material_path):
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_DuelColor", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -300, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.3, 0.4, 0.5, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)

sound_path = "/Game/Audio/S_Fire"
if not unreal.EditorAssetLibrary.does_asset_exist(sound_path):
    scratch = os.path.join(project, "Saved", "GeneratedAssets")
    os.makedirs(scratch, exist_ok=True)
    audio = os.path.join(scratch, "S_Fire.wav")
    sample_rate = 22050
    rng = random.Random(17)
    with wave.open(audio, "wb") as output:
        output.setparams((1, 2, sample_rate, 0, "NONE", "not compressed"))
        frames = []
        for i in range(int(sample_rate * 0.07)):
            t = i / sample_rate
            pulse = (0.65 * rng.uniform(-1, 1) + 0.35 * math.sin(2 * math.pi * 150 * t)) * math.exp(-t * 65)
            frames.append(struct.pack("<h", int(pulse * 20000)))
        output.writeframes(b"".join(frames))
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", audio)
    task.set_editor_property("destination_path", "/Game/Audio")
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if not unreal.EditorAssetLibrary.does_asset_exist(sound_path):
        raise RuntimeError("Audio import failed")

for name in ("L_Menu", "L_Arena"):
    path = "/Game/Maps/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        if not unreal.EditorLevelLibrary.new_level(path):
            raise RuntimeError("Could not create map " + path)
        if not unreal.EditorLevelLibrary.save_current_level():
            raise RuntimeError("Could not save map " + path)

# Small PBR library built from the engine's bundled Starter Content textures.
# File names are stable so repeated Bootstrap runs preserve any hand edits.
library = unreal.MaterialEditingLibrary
for name, texture, roughness, metallic in (
        ("Concrete", "Concrete_Poured", 0.88, 0.0),
        ("Brick", "Brick_Clay_Old", 0.9, 0.0),
        ("Wood", "Wood_Pine", 0.82, 0.0),
        ("Metal", "Metal_Steel", 0.48, 0.65),
        ("Paint", None, 0.68, 0.1)):
    path = "/Game/Materials/M_" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        if name == "Paint":
            existing = unreal.EditorAssetLibrary.load_asset(path)
            if not existing.get_editor_property("used_with_skeletal_mesh"):
                existing.set_editor_property("used_with_skeletal_mesh", True)
                library.recompile_material(existing)
                if not unreal.EditorAssetLibrary.save_loaded_asset(existing):
                    raise RuntimeError("Could not save skeletal-mesh material usage")
        continue
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_" + name, "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    if name == "Paint":
        mat.set_editor_property("used_with_skeletal_mesh", True)
    color = library.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    if texture:
        uv = library.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1000, 200)
        tile = library.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1000, 350)
        tile.set_editor_property("parameter_name", "Tiling")
        tile.set_editor_property("default_value", 1.0)
        coords = library.create_material_expression(mat, unreal.MaterialExpressionMultiply, -800, 200)
        library.connect_material_expressions(uv, "", coords, "A")
        library.connect_material_expressions(tile, "", coords, "B")
        for suffix, prop in (("D", unreal.MaterialProperty.MP_BASE_COLOR), ("N", unreal.MaterialProperty.MP_NORMAL)):
            tex = unreal.EditorAssetLibrary.load_asset("/Game/StarterContent/Textures/T_" + texture + "_" + suffix)
            if not tex:
                raise RuntimeError("Missing bundled texture: " + texture + "_" + suffix)
            tex.set_editor_property("max_texture_size", 1024)
            unreal.EditorAssetLibrary.save_loaded_asset(tex)
            sample = library.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 200 if suffix == "D" else 500)
            sample.set_editor_property("texture", tex)
            if suffix == "N":
                sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
            library.connect_material_expressions(coords, "", sample, "UVs")
            if suffix == "D":
                tint = library.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 0)
                library.connect_material_expressions(sample, "RGB", tint, "A")
                library.connect_material_expressions(color, "", tint, "B")
                library.connect_material_property(tint, "", prop)
            else:
                library.connect_material_property(sample, "RGB", prop)
    else:
        library.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    for value, prop in ((roughness, unreal.MaterialProperty.MP_ROUGHNESS), (metallic, unreal.MaterialProperty.MP_METALLIC)):
        expression = library.create_material_expression(mat, unreal.MaterialExpressionConstant, -200, 700)
        expression.set_editor_property("r", value)
        library.connect_material_property(expression, "", prop)
    library.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)

if not unreal.EditorAssetLibrary.does_asset_exist("/Game/Materials/M_Sky"):
    sky = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_Sky", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    sky.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    sky.set_editor_property("two_sided", True)
    color = library.create_material_expression(sky, unreal.MaterialExpressionConstant3Vector, -200, 0)
    color.set_editor_property("constant", unreal.LinearColor(0.35, 0.52, 0.7, 1))
    library.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.recompile_material(sky)
    unreal.EditorAssetLibrary.save_loaded_asset(sky)

if os.path.isfile(os.path.join(project, "Assets", "Source", "PolyHaven", "concrete_floor_worn_001", "manifest.json")):
    import runpy
    runpy.run_path(os.path.join(project, "tools", "ImportFreeAssets.py"))

quantum_source = os.path.join(project, "Assets", "Source", "Fab", "Quantum")
if os.path.isfile(os.path.join(quantum_source, "FBX", "SKM_Character.fbx")):
    import runpy
    runpy.run_path(os.path.join(project, "tools", "ImportQuantumAssets.py"))
    if os.path.isfile(os.path.join(quantum_source, "RecoveredTextures.json")):
        runpy.run_path(os.path.join(project, "tools", "PrepareQuantumAssets.py"))
        runpy.run_path(os.path.join(project, "tools", "PrepareCombatAssets.py"))

if os.path.isfile(os.path.join(project, "Assets", "Source", "PolyHaven", "modular_factory_facade", "manifest.json")):
    import runpy
    runpy.run_path(os.path.join(project, "tools", "ImportWarehouseAssets.py"))

unreal.log("TPSDuel: maps, warehouse materials and acquired character assets initialized.")
