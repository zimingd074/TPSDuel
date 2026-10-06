"""Prepare selected Factory meshes for the small PC / Android arena."""
import json
import os
import unreal

project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
migration_path = os.path.join(project, "Saved", "FactoryMigration.json")
if not os.path.isfile(migration_path):
    migration_path = os.path.join(project, "Assets", "Source", "Fab", "FactoryEnvironmentCollect", "TPSDuelMigration.json")
with open(migration_path, encoding="utf-8") as stream:
    migration = json.load(stream)
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous(["/Game/Meshes", "/Game/Materials", "/Game/Textures"], True)
for entry in migration["files"]:
    asset = unreal.EditorAssetLibrary.load_asset(entry["package"])
    if not asset:
        raise RuntimeError("Missing migrated dependency: " + entry["package"])
    if isinstance(asset, unreal.Texture):
        asset.set_editor_property("max_texture_size", 1024)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
meshes = {}
for role, path in migration["selection"].items():
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Not a static mesh: " + path)
    unreal.EditorStaticMeshLibrary.remove_collisions(mesh)
    unreal.EditorStaticMeshLibrary.add_simple_collisions(mesh, unreal.ScriptingCollisionShapeType.BOX)
    if unreal.EditorStaticMeshLibrary.get_lod_count(mesh) < 3:
        settings = []
        for fraction, screen in ((1.0, 1.0), (0.5, 0.35), (0.2, 0.12)):
            setting = unreal.EditorScriptingMeshReductionSettings()
            setting.set_editor_property("percent_triangles", fraction)
            setting.set_editor_property("screen_size", screen)
            settings.append(setting)
        options = unreal.EditorScriptingMeshReductionOptions()
        options.set_editor_property("reduction_settings", settings)
        options.set_editor_property("auto_compute_lod_screen_size", False)
        if unreal.EditorStaticMeshLibrary.set_lods(mesh, options) < 3:
            raise RuntimeError("Factory LOD generation failed: " + path)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    bounds = mesh.get_bounds()
    meshes[role] = {"path": path, "origin": [bounds.origin.x, bounds.origin.y, bounds.origin.z],
                    "extent": [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z],
                    "lods": unreal.EditorStaticMeshLibrary.get_lod_count(mesh),
                    "materials": [slot.material_interface.get_path_name() for slot in mesh.static_materials]}
with open(os.path.join(project, "Saved", "FactoryImport.json"), "w", encoding="utf-8") as stream:
    json.dump({"state": "prepared", "source": "Factory Environment Collection UE4.27",
               "textures_max_size": 1024, "meshes": meshes}, stream, indent=2)
unreal.log("TPSDuel: eight selected Factory meshes prepared")
