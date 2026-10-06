"""Adapt the locally acquired Fab sample for UE4.27 gameplay.

Use the CRC-checked full texture pack when acquired; retain the documented
arms-only fallback for checkouts without the complete pack.
"""
import json
import os
import hashlib
import unreal

project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
destination = "/Game/ThirdParty/Quantum"
report_path = os.path.join(project, "Saved", "QuantumPreparation.json")
with open(report_path, "w", encoding="utf-8") as stream:
    json.dump({"state": "running"}, stream)
mesh = unreal.EditorAssetLibrary.load_asset(destination + "/SKM_Character")
rifle = unreal.EditorAssetLibrary.load_asset(destination + "/SM_Rifle")
if not mesh or not rifle:
    raise RuntimeError("Run ImportQuantumAssets.py first")
tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.MaterialEditingLibrary
textures = {}
source = os.path.join(project, "Assets", "Source", "Fab", "Quantum")
recovered_path = os.path.join(source, "RecoveredTextures.json")
recovered = {"files": []}
if os.path.isfile(recovered_path):
    with open(recovered_path, encoding="utf-8") as stream:
        recovered = json.load(stream)
for entry in recovered["files"]:
    key = "BaseColor" if "BaseColor" in entry["file"] else "Normal" if "Normal" in entry["file"] else "ORM"
    path = destination + "/Textures/T_Arms_" + key
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(source, entry["file"]))
        task.set_editor_property("destination_path", destination + "/Textures")
        task.set_editor_property("destination_name", "T_Arms_" + key)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        tools.import_asset_tasks([task])
    tex = unreal.EditorAssetLibrary.load_asset(path)
    tex.set_editor_property("max_texture_size", 1024)
    tex.set_editor_property("srgb", key == "BaseColor")
    if key != "BaseColor":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP if key == "Normal" else unreal.TextureCompressionSettings.TC_MASKS)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    textures[key] = tex

full_path = os.path.join(source, "FullTextures.json")
full = None
if os.path.isfile(full_path):
    with open(full_path, encoding="utf-8") as stream:
        full = json.load(stream)
    if not full.get("complete_pack"):
        raise RuntimeError("Full texture manifest is not complete")
    for entry in full["files"]:
        with open(os.path.join(source, entry["file"]), "rb") as stream:
            if hashlib.sha256(stream.read()).hexdigest() != entry["sha256"]:
                raise RuntimeError("Quantum texture checksum failed: " + entry["file"])
if not full and len(textures) != 3:
    raise RuntimeError("Acquire the complete PBR pack, or provide the verified arms fallback")

original_groups = {
    "M_Cap": ("Cap", "T_Cap_Bege", "T_Cap"),
    "M_Holster_Hard": ("Holdster_Hard", "M_Holster_Hard_Bege", "M_Holster_Hard"),
    "M_Eyelashesh": ("Body", "T_Eyelashesh", "T_Eyelashesh"),
    "M_Head": ("Body", "T_Head", "T_Head"),
    "M_Shirt_RolledUp": ("Shirt_RolledUp", "T_Shirt_RolledUp_Blue", "T_Shirt_RolledUp"),
    "M_Bulletproof_Light": ("Bulletproof", "T_Bulletproof_Bege", "T_Bulletproof_Light"),
    "M_Jeans": ("Jeans", "T_Jeans_Bege", "T_Jeans"),
    "M_Quantum_Arms": ("Arms", "T_Quantum_Basemesh_Arms", "T_Quantum_Basemesh_Arms"),
    "M_Eyeball": ("Body", "T_Eyeball", "T_Eyeball"),
    "M_Sclera": ("Body", "T_Sclera", "T_Sclera"),
    "M_Teeth": ("Body", "T_Teeth", "T_Teeth"),
    "M_Pistol": ("Weapon", "Pistol", "Pistol"),
    "M_Patches": ("Patch", "T_Patches", "T_Patches"),
    "M_Drops_Tactical": ("Drops", "T_Drops_Tactical_Bege", "T_Drops_Tactical"),
    "M_Rifle": ("Weapon", "Rifle_Olive", "Rifle_Olive"),
}


def original_textures(name):
    group, base_prefix, prefix = original_groups[name]
    result = {}
    for key, suffix in (("BaseColor", "BaseColor"), ("Normal", "Normal"), ("ORM", "OcclusionRoughnessMetallic")):
        stem = (base_prefix if key == "BaseColor" else prefix) + "_" + suffix
        candidates = [entry for entry in full["files"] if entry["file"].startswith("PBR_Textures/" + group + "/" + stem + ".")]
        if len(candidates) != 1:
            raise RuntimeError("Missing/ambiguous original texture: " + name + "/" + key)
        tex_name = "T_Original_" + name[2:] + "_" + key
        path = destination + "/Textures/" + tex_name
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            task = unreal.AssetImportTask()
            task.filename = os.path.join(source, candidates[0]["file"])
            task.destination_path = destination + "/Textures"
            task.destination_name = tex_name
            task.automated = True
            task.save = True
            tools.import_asset_tasks([task])
        tex = unreal.EditorAssetLibrary.load_asset(path)
        tex.set_editor_property("max_texture_size", 2048 if name == "M_Head" else 1024)
        tex.set_editor_property("srgb", key == "BaseColor")
        if key != "BaseColor":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP if key == "Normal" else unreal.TextureCompressionSettings.TC_MASKS)
        unreal.EditorAssetLibrary.save_loaded_asset(tex)
        result[key] = tex
    return result


def material(name, colour, roughness=0.8, metallic=0.0, arms=False, hidden=False):
    path = destination + "/Materials/" + name
    mat = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name, destination + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    library.delete_all_material_expressions(mat)
    mat.set_editor_property("used_with_skeletal_mesh", True)
    maps = original_textures(name) if full else textures if arms else None
    if full:
        hidden = False
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED if name == "M_Eyelashesh" else unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("two_sided", name == "M_Eyelashesh")
    if hidden:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        mask = library.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 200)
        mask.set_editor_property("r", 0.0)
        library.connect_material_property(mask, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    if maps:
        for row, key in enumerate(("BaseColor", "Normal", "ORM")):
            sample = library.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, row * 250)
            sample.set_editor_property("texture", maps[key])
            if key == "BaseColor":
                library.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
                if name == "M_Eyelashesh":
                    library.connect_material_property(sample, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
                if name == "M_Patches":
                    tint = library.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -350, -200)
                    tint.set_editor_property("parameter_name", "Color")
                    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
                    multiply = library.create_material_expression(mat, unreal.MaterialExpressionMultiply, -100, 0)
                    library.connect_material_expressions(sample, "RGB", multiply, "A")
                    library.connect_material_expressions(tint, "", multiply, "B")
                    library.connect_material_property(multiply, "", unreal.MaterialProperty.MP_BASE_COLOR)
            elif key == "Normal":
                sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
                library.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_NORMAL)
            else:
                sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
                for output, prop in (("R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION), ("G", unreal.MaterialProperty.MP_ROUGHNESS), ("B", unreal.MaterialProperty.MP_METALLIC)):
                    library.connect_material_property(sample, output, prop)
    else:
        colour_node = library.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 0)
        colour_node.set_editor_property("parameter_name", "Color")
        colour_node.set_editor_property("default_value", unreal.LinearColor(*colour, 1.0))
        library.connect_material_property(colour_node, "", unreal.MaterialProperty.MP_BASE_COLOR)
        for row, (value, prop) in enumerate(((roughness, unreal.MaterialProperty.MP_ROUGHNESS), (metallic, unreal.MaterialProperty.MP_METALLIC))):
            node = library.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 250 + row * 150)
            node.set_editor_property("r", value)
            library.connect_material_property(node, "", prop)
    library.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


styles = {
    "M_Cap": ((0.06, 0.08, 0.04), 0.88, 0.0),
    "M_Holster_Hard": ((0.025, 0.025, 0.021), 0.65, 0.0),
    "M_Eyelashesh": ((0.02, 0.015, 0.012), 0.9, 0.0),
    "M_Head": ((0.42, 0.23, 0.15), 0.62, 0.0),
    "M_Shirt_RolledUp": ((0.12, 0.15, 0.10), 0.95, 0.0),
    "M_Bulletproof_Light": ((0.17, 0.13, 0.07), 0.9, 0.0),
    "M_Jeans": ((0.025, 0.045, 0.07), 0.96, 0.0),
    "M_Quantum_Arms": ((0.42, 0.23, 0.15), 0.62, 0.0),
    "M_Eyeball": ((0.015, 0.025, 0.025), 0.18, 0.0),
    "M_Sclera": ((0.7, 0.65, 0.58), 0.22, 0.0),
    "M_Teeth": ((0.6, 0.54, 0.4), 0.5, 0.0),
    "M_Pistol": ((0.018, 0.02, 0.022), 0.4, 0.7),
    "M_Patches": ((0.09, 0.25, 0.7), 0.9, 0.0),
    "M_Drops_Tactical": ((0.08, 0.09, 0.05), 0.85, 0.0),
}
slots = list(mesh.get_editor_property("materials"))
for slot in slots:
    name = str(slot.get_editor_property("material_slot_name"))
    if name not in styles:
        raise RuntimeError("Unmapped character material: " + name)
    colour, roughness, metallic = styles[name]
    slot.set_editor_property("material_interface", material(name, colour, roughness, metallic, arms=name == "M_Quantum_Arms", hidden=name == "M_Eyelashesh"))
mesh.set_editor_property("materials", slots)
for slot in mesh.get_editor_property("materials"):
    if not slot.get_editor_property("material_interface"):
        raise RuntimeError("Character material assignment failed: " + str(slot.get_editor_property("material_slot_name")))
if unreal.EditorSkeletalMeshLibrary.get_lod_count(mesh) < 3:
    if not unreal.EditorSkeletalMeshLibrary.regenerate_lod(mesh, 3):
        raise RuntimeError("Character LOD generation failed")
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
rifle.set_material(0, material("M_Rifle", (0.015, 0.018, 0.022), 0.5, 0.6))
if unreal.EditorStaticMeshLibrary.get_lod_count(rifle) < 3:
    reductions = []
    for fraction, screen in ((1.0, 1.0), (0.5, 0.35), (0.2, 0.12)):
        setting = unreal.EditorScriptingMeshReductionSettings()
        setting.set_editor_property("percent_triangles", fraction)
        setting.set_editor_property("screen_size", screen)
        reductions.append(setting)
    options = unreal.EditorScriptingMeshReductionOptions()
    options.set_editor_property("reduction_settings", reductions)
    options.set_editor_property("auto_compute_lod_screen_size", False)
    if unreal.EditorStaticMeshLibrary.set_lods(rifle, options) != 3:
        raise RuntimeError("Rifle LOD generation failed")
unreal.EditorAssetLibrary.save_loaded_asset(rifle)
animation_path = destination + "/Animations/Q_ThirdPerson_AnimBP"
if not unreal.EditorAssetLibrary.does_asset_exist(animation_path):
    animations = list(unreal.DuelAssetLibrary.retarget_quantum_locomotion())
    if not animations:
        raise RuntimeError("Animation retargeting failed")
if not unreal.DuelAssetLibrary.make_quantum_hold_pose():
    raise RuntimeError("Initial upper-body holding pose failed")
minimum_lod = unreal.PerPlatformInt()
minimum_lod.set_editor_property("default", 1)
mesh.set_editor_property("min_lod", minimum_lod)
unreal.EditorAssetLibrary.save_directory(destination, only_if_is_dirty=True, recursive=True)
blueprint = unreal.EditorAssetLibrary.load_asset(animation_path)
if not blueprint or blueprint.get_editor_property("target_skeleton") != mesh.get_editor_property("skeleton"):
    raise RuntimeError("Retargeted AnimBP skeleton mismatch")
bounds = rifle.get_bounds()
report = {"state": "prepared", "full_original_textures": bool(full), "arms_original_textures": True, "temporary_materials": [] if full else [key for key in styles if key != "M_Quantum_Arms"], "materials": [slot.get_editor_property("material_interface").get_path_name() for slot in mesh.get_editor_property("materials")], "animation": animation_path, "holding_pose": "baked initial pose; runtime grip, carry and reload installed by PrepareCombatAssets.py", "min_lod": 1, "rifle_bounds": {"origin": [bounds.origin.x, bounds.origin.y, bounds.origin.z], "extent": [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z]}, "bones": list(unreal.DuelAssetLibrary.describe_quantum_bones()), "lod_vertices": [unreal.EditorSkeletalMeshLibrary.get_num_verts(mesh, index) for index in range(3)], "lod_sections": [unreal.EditorSkeletalMeshLibrary.get_num_sections(mesh, index) for index in range(3)]}
with open(report_path, "w", encoding="utf-8") as stream:
    json.dump(report, stream, indent=2)
unreal.log("TPSDuel: Quantum mesh/materials/locomotion prepared; original textures={}".format(bool(full)))
