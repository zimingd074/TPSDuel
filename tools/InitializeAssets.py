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

unreal.log("TPSDuel: maps, unlit material and original placeholder fire sound initialized.")
