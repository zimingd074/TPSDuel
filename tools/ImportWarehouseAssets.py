"""Import CC0 factory modules, warehouse props and plaster; verify every source."""
import hashlib
import json
import os
import unreal

project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.MaterialEditingLibrary
base = "/Game/FreeAssets/PolyHaven/Warehouse"
report = []


def load_manifest(asset):
    folder = os.path.join(project, "Assets", "Source", "PolyHaven", asset)
    with open(os.path.join(folder, "manifest.json"), encoding="utf-8-sig") as stream:
        manifest = json.load(stream)
    for entry in manifest["files"]:
        with open(os.path.join(folder, entry["file"]), "rb") as stream:
            if hashlib.sha256(stream.read()).hexdigest().lower() != entry["sha256"].lower():
                raise RuntimeError("Source checksum mismatch: " + entry["file"])
    return folder, manifest


def texture(folder, entry, name):
    path = base + "/Textures/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        task = unreal.AssetImportTask()
        task.filename = os.path.join(folder, entry["file"])
        task.destination_path = base + "/Textures"
        task.destination_name = name
        task.automated = True
        task.save = True
        tools.import_asset_tasks([task])
    tex = unreal.EditorAssetLibrary.load_asset(path)
    tex.set_editor_property("max_texture_size", 1024)
    tex.set_editor_property("srgb", entry["map"].endswith("diff") or entry["map"] == "Diffuse")
    if "nor_dx" in entry["map"]:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    elif "arm" in entry["map"]:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    return tex


def material(name, maps, plaster=False):
    path = base + "/Materials/M_" + name
    mat = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset("M_" + name, base + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    library.delete_all_material_expressions(mat)
    if name.startswith("Factory"):
        mat.set_editor_property("two_sided", True)
    # Plaster cubes use mesh UVs; tiling is configurable for walls/stair risers.
    uv = None
    if plaster:
        coords = library.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, 0)
        tiling = library.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 200)
        tiling.set_editor_property("parameter_name", "Tiling")
        tiling.set_editor_property("default_value", 4.0)
        uv = library.create_material_expression(mat, unreal.MaterialExpressionMultiply, -650, 0)
        library.connect_material_expressions(coords, "", uv, "A")
        library.connect_material_expressions(tiling, "", uv, "B")
    for row, key in enumerate(("diff", "nor_dx", "arm")):
        sample = library.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, row * 200)
        sample.set_editor_property("texture", maps[key])
        if uv:
            library.connect_material_expressions(uv, "", sample, "UVs")
        if key == "diff":
            library.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        elif key == "nor_dx":
            sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
            library.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_NORMAL)
        else:
            sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
            for channel, prop in (("R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION), ("G", unreal.MaterialProperty.MP_ROUGHNESS), ("B", unreal.MaterialProperty.MP_METALLIC)):
                library.connect_material_property(sample, channel, prop)
    library.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


for asset, prefix in (("modular_factory_facade", "Factory"), ("concrete_road_barrier", "RoadBarrier"), ("industrial_storage_cart", "StorageCart"), ("yellow_plaster_02", "SandPlaster")):
    folder, manifest = load_manifest(asset)
    materials = {}
    groups = ("brick", "doors", "garage", "windows", "trim_01") if prefix == "Factory" else ("",)
    for group in groups:
        maps = {}
        for entry in manifest["files"]:
            for key in ("diff", "nor_dx", "arm"):
                expected = group + "_" + key if group else "Diffuse" if key == "diff" else key
                if entry["map"] == expected:
                    maps[key] = texture(folder, entry, "T_" + prefix + "_" + (group + "_" if group else "") + key)
        if len(maps) != 3:
            raise RuntimeError("Missing material maps: " + asset + "/" + group)
        materials[group] = material(prefix + ("_" + group if group else ""), maps, prefix == "SandPlaster")
    meshes = []
    if prefix != "SandPlaster":
        mesh_dir = base + "/" + prefix
        if not unreal.EditorAssetLibrary.list_assets(mesh_dir, recursive=True, include_folder=False):
            entry = next(item for item in manifest["files"] if item["map"] == "fbx")
            options = unreal.FbxImportUI()
            options.set_editor_property("import_mesh", True)
            options.set_editor_property("import_as_skeletal", False)
            options.set_editor_property("import_materials", False)
            options.set_editor_property("import_textures", False)
            options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
            options.static_mesh_import_data.set_editor_property("combine_meshes", prefix != "Factory")
            options.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
            task = unreal.AssetImportTask()
            task.filename = os.path.join(folder, entry["file"])
            task.destination_path = mesh_dir
            task.destination_name = "SM_" + prefix
            task.options = options
            task.automated = True
            task.save = True
            tools.import_asset_tasks([task])
        for path in unreal.EditorAssetLibrary.list_assets(mesh_dir, recursive=True, include_folder=False):
            mesh = unreal.EditorAssetLibrary.load_asset(path)
            if not isinstance(mesh, unreal.StaticMesh):
                continue
            for index, slot in enumerate(mesh.get_editor_property("static_materials")):
                slot_name = str(slot.get_editor_property("material_slot_name")).lower()
                if prefix == "Factory":
                    matches = [key for key in materials if key in slot_name]
                    if not matches:
                        raise RuntimeError("Unmapped factory slot: " + slot_name)
                    mat = materials["doors"] if slot_name.endswith("brick_doors") else materials[matches[0]]
                else:
                    mat = materials[""]
                mesh.set_material(index, mat)
            if unreal.EditorStaticMeshLibrary.get_lod_count(mesh) < 3:
                options = unreal.EditorScriptingMeshReductionOptions()
                settings = []
                for fraction, screen in ((1.0, 1.0), (0.5, 0.35), (0.2, 0.12)):
                    setting = unreal.EditorScriptingMeshReductionSettings()
                    setting.percent_triangles = fraction
                    setting.screen_size = screen
                    settings.append(setting)
                options.reduction_settings = settings
                options.auto_compute_lod_screen_size = False
                unreal.EditorStaticMeshLibrary.set_lods(mesh, options)
                unreal.EditorStaticMeshLibrary.add_simple_collisions(mesh, unreal.ScriptingCollisionShapeType.BOX)
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)
            bounds = mesh.get_bounds()
            meshes.append({"path": mesh.get_path_name(), "origin": [bounds.origin.x, bounds.origin.y, bounds.origin.z], "extent": [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z], "slots": [str(slot.material_slot_name) for slot in mesh.static_materials], "lods": unreal.EditorStaticMeshLibrary.get_lod_count(mesh)})
    report.append({"asset": asset, "license": "CC0", "source": manifest["source"], "meshes": meshes, "materials": {key or "surface": mat.get_path_name() for key, mat in materials.items()}})

with open(os.path.join(project, "Saved", "WarehouseImport.json"), "w", encoding="utf-8") as stream:
    json.dump({"state": "complete", "assets": report}, stream, indent=2)
unreal.log("TPSDuel: CC0 warehouse import complete")
