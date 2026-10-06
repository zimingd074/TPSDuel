"""Run in the downloaded UE4.27 Factory project; inspect assets without migration."""
import json
import os
import unreal

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous(["/Game"], True)
assets = registry.get_assets_by_path("/Game", recursive=True)
meshes = [str(asset.object_path) for asset in assets if str(asset.asset_class) == "StaticMesh"]
report = {"source_project_dir": unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
          "source_content": unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()),
          "meshes": sorted(meshes)}
with open("D:/TPSDuel/Saved/FactoryCatalog.json", "w", encoding="utf-8") as stream:
    json.dump(report, stream, indent=2)
unreal.log("TPSDuel: factory catalog contains {} meshes".format(len(meshes)))
