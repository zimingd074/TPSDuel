"""Stage licensed Fab FBX meshes locally; do not replace the playable pawn yet.

The sample uses a UE5 skeleton. Materials and animation retargeting must be
verified before enabling it in UE4.27. Source/content directories are ignored.
"""
import json
import os
import unreal

project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
source = os.path.join(project, "Assets", "Source", "Fab", "Quantum", "FBX")
destination = "/Game/ThirdParty/Quantum"
report_path = os.path.join(project, "Saved", "QuantumImport.json")
with open(report_path, "w", encoding="utf-8") as stream:
    json.dump({"state": "running"}, stream)
reports = []
for name, skeletal in (("SKM_Character", True), ("SM_Rifle", False)):
    path = destination + "/" + name
    existing = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not existing or (skeletal and not existing.get_editor_property("skeleton")):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(source, name + ".fbx"))
        task.set_editor_property("destination_path", destination)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("replace_existing", True)
        options = unreal.FbxImportUI()
        options.set_editor_property("automated_import_should_detect_type", False)
        options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH if skeletal else unreal.FBXImportType.FBXIT_STATIC_MESH)
        options.set_editor_property("import_as_skeletal", skeletal)
        options.set_editor_property("import_animations", False)
        options.set_editor_property("create_physics_asset", False)
        options.set_editor_property("import_materials", False)
        options.set_editor_property("import_textures", False)
        data = options.get_editor_property("skeletal_mesh_import_data" if skeletal else "static_mesh_import_data")
        data.set_editor_property("convert_scene_unit", True)
        if not skeletal:
            data.set_editor_property("combine_meshes", True)
            data.set_editor_property("auto_generate_collision", False)
        task.set_editor_property("options", options)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    expected = unreal.SkeletalMesh if skeletal else unreal.StaticMesh
    if not isinstance(mesh, expected):
        raise RuntimeError("FBX import failed: " + path)
    slots = mesh.get_editor_property("materials" if skeletal else "static_materials")
    result = {"asset": mesh.get_path_name(), "materials": [str(slot.get_editor_property("material_slot_name")) for slot in slots]}
    if skeletal:
        result["skeleton"] = mesh.get_editor_property("skeleton").get_path_name()
        result["lods"] = unreal.EditorSkeletalMeshLibrary.get_lod_count(mesh)
    else:
        result["lods"] = unreal.EditorStaticMeshLibrary.get_lod_count(mesh)
    reports.append(result)
unreal.EditorAssetLibrary.save_directory(destination, only_if_is_dirty=True, recursive=True)
with open(report_path, "w", encoding="utf-8") as stream:
    json.dump({"state": "staged", "playable": False, "reason": "Requires PBR textures and UE4 animation retargeting", "assets": reports}, stream, ensure_ascii=False, indent=2)
unreal.log("TPSDuel: Quantum FBX staged; playable pawn remains unchanged until texture/animation validation.")
