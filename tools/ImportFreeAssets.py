"""Import official CC0 scans and FBX props using UE4.27 Editor Python."""
import json
import os
import unreal

project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.MaterialEditingLibrary
reports = []
report_path = os.path.join(project, "Saved", "FreeAssetsImport.json")
with open(report_path, "w", encoding="utf-8") as stream:
    json.dump({"state": "running"}, stream)


def import_textures(source, manifest, name):
    textures = {}
    suffixes = {"Diffuse": "BaseColor", "nor_dx": "Normal", "arm": "ARM"}
    destination = "/Game/FreeAssets/PolyHaven/" + ("ConcreteFloor" if name == "WornFloor" else name)
    for entry in manifest["files"]:
        key = entry["map"]
        if key not in suffixes:
            continue
        asset_name = "T_" + name + "_" + suffixes[key]
        path = destination + "/" + asset_name
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", os.path.join(source, entry["file"]))
            task.set_editor_property("destination_path", destination)
            task.set_editor_property("destination_name", asset_name)
            task.set_editor_property("automated", True)
            task.set_editor_property("save", True)
            tools.import_asset_tasks([task])
        texture = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(texture, unreal.Texture2D):
            raise RuntimeError("Texture import failed: " + path)
        texture.set_editor_property("max_texture_size", 1024)
        texture.set_editor_property("srgb", key == "Diffuse")
        if key == "nor_dx":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        elif key == "arm":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
            raise RuntimeError("Could not save " + path)
        textures[key] = texture
    if len(textures) != 3:
        raise RuntimeError("Missing PBR textures for " + name)
    return textures


def create_material(name, textures, floor=False):
    path = "/Game/Materials/M_" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        material = unreal.EditorAssetLibrary.load_asset(path)
        library.delete_all_material_expressions(material)
    else:
        material = tools.create_asset("M_" + name, "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    uv = None
    if floor:
        # XY world coordinates preserve the scan's 3m repeat on a 36x24m floor.
        position = library.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1000, 0)
        xy = library.create_material_expression(material, unreal.MaterialExpressionComponentMask, -800, 0)
        xy.set_editor_property("r", True)
        xy.set_editor_property("g", True)
        if not library.connect_material_expressions(position, "", xy, ""):
            raise RuntimeError("Could not connect world-position floor coordinates")
        scale = library.create_material_expression(material, unreal.MaterialExpressionConstant, -800, 200)
        scale.set_editor_property("r", 1.0 / 300.0)
        uv = library.create_material_expression(material, unreal.MaterialExpressionMultiply, -600, 0)
        library.connect_material_expressions(xy, "", uv, "A")
        library.connect_material_expressions(scale, "", uv, "B")
    for index, key in enumerate(("Diffuse", "nor_dx", "arm")):
        sample = library.create_material_expression(material, unreal.MaterialExpressionTextureSample, -400, index * 300)
        sample.set_editor_property("texture", textures[key])
        if key == "nor_dx":
            sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        elif key == "arm":
            sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        if uv:
            library.connect_material_expressions(uv, "", sample, "UVs")
        if key == "Diffuse":
            library.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        elif key == "nor_dx":
            library.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_NORMAL)
        else:
            for channel, prop in (("R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION), ("G", unreal.MaterialProperty.MP_ROUGHNESS), ("B", unreal.MaterialProperty.MP_METALLIC)):
                library.connect_material_property(sample, channel, prop)
    library.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Could not save " + path)
    return material


def import_mesh(source, manifest, name, material):
    entry = next(item for item in manifest["files"] if item["map"] == "fbx")
    destination = "/Game/FreeAssets/PolyHaven/" + name
    path = destination + "/SM_" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(source, entry["file"]))
        task.set_editor_property("destination_path", destination)
        task.set_editor_property("destination_name", "SM_" + name)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        options = unreal.FbxImportUI()
        options.set_editor_property("automated_import_should_detect_type", False)
        options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        options.set_editor_property("import_materials", False)
        options.set_editor_property("import_textures", False)
        data = options.get_editor_property("static_mesh_import_data")
        data.set_editor_property("combine_meshes", True)
        data.set_editor_property("auto_generate_collision", False)
        data.set_editor_property("convert_scene_unit", True)
        task.set_editor_property("options", options)
        tools.import_asset_tasks([task])
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            raise RuntimeError("FBX import failed: " + path)
        reductions = []
        for percent, screen in ((1.0, 1.0), (0.5, 0.35), (0.2, 0.12)):
            setting = unreal.EditorScriptingMeshReductionSettings()
            setting.set_editor_property("percent_triangles", percent)
            setting.set_editor_property("screen_size", screen)
            reductions.append(setting)
        reduction_options = unreal.EditorScriptingMeshReductionOptions()
        reduction_options.set_editor_property("auto_compute_lod_screen_size", False)
        reduction_options.set_editor_property("reduction_settings", reductions)
        if unreal.EditorStaticMeshLibrary.set_lods(mesh, reduction_options) != 3:
            raise RuntimeError("Could not generate three LODs: " + path)
        unreal.EditorStaticMeshLibrary.add_simple_collisions(mesh, unreal.ScriptingCollisionShapeType.BOX)
    else:
        mesh = unreal.EditorAssetLibrary.load_asset(path)
    for index in range(len(mesh.get_editor_property("static_materials"))):
        mesh.set_material(index, material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mesh):
        raise RuntimeError("Could not save " + path)
    bounds = mesh.get_bounds()
    return {"asset": path, "lods": unreal.EditorStaticMeshLibrary.get_lod_count(mesh), "sections": [mesh.get_num_sections(i) for i in range(3)], "origin": [bounds.origin.x, bounds.origin.y, bounds.origin.z], "extent": [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z]}


for asset, texture_name, material_name in (("concrete_floor_worn_001", "WornFloor", "WornFloor"), ("barrel_03", "Barrel", "Barrel"), ("wooden_military_crate", "MilitaryCrate", "MilitaryCrate")):
    source = os.path.join(project, "Assets", "Source", "PolyHaven", asset)
    manifest_path = os.path.join(source, "manifest.json")
    if not os.path.isfile(manifest_path):
        raise RuntimeError("Run tools/FetchFreeAssets.ps1 before importing: " + asset)
    with open(manifest_path, encoding="utf-8-sig") as stream:
        manifest = json.load(stream)
    textures = import_textures(source, manifest, texture_name)
    material = create_material(material_name, textures, floor=asset == "concrete_floor_worn_001")
    report = {"source": manifest["source"], "license": "CC0", "material": material.get_path_name(), "textures": {key: value.get_path_name() for key, value in textures.items()}, "resolution": 1024}
    if asset != "concrete_floor_worn_001":
        report["mesh"] = import_mesh(source, manifest, texture_name, material)
    reports.append(report)
    unreal.log("TPSDuel: free asset imported: " + asset)

with open(report_path, "w", encoding="utf-8") as stream:
    json.dump({"state": "complete", "assets": reports}, stream, ensure_ascii=False, indent=2)
unreal.log("TPSDuel: CC0 asset import complete.")
