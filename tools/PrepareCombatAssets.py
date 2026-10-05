"""Install the native two-hand weapon controller on the duplicated AnimBP."""
import json
import os
import unreal

project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
report_path = os.path.join(project, "Saved", "CombatAssets.json")
with open(report_path, "w", encoding="utf-8") as stream:
    json.dump({"state": "running"}, stream)
if not unreal.DuelAssetLibrary.configure_quantum_locomotion():
    raise RuntimeError("Could not configure directional locomotion")
if not unreal.DuelAssetLibrary.split_quantum_rifle():
    raise RuntimeError("Could not separate rifle magazine")
for name in ("SM_RifleBody", "SM_RifleMagazine"):
    if not unreal.EditorAssetLibrary.save_asset("/Game/ThirdParty/Quantum/" + name):
        raise RuntimeError("Could not save " + name)
if not unreal.DuelAssetLibrary.install_quantum_weapon_pose():
    raise RuntimeError("Could not install weapon animation node")
unreal.EditorAssetLibrary.save_asset("/Game/ThirdParty/Quantum/Animations/Q_ThirdPerson_AnimBP")
with open(os.path.join(project, "Saved", "LocomotionGraph.json"), "w", encoding="utf-8") as stream:
    json.dump(list(unreal.DuelAssetLibrary.describe_locomotion()), stream, indent=2, ensure_ascii=False)
with open(report_path, "w", encoding="utf-8") as stream:
    json.dump({"state": "prepared", "weapon_pose": "native two-bone IK, grip fingers, chest carry and procedural magazine reload", "locomotion": "reverse backward cycle and directional foot IK", "pose_sync": "shared animation snapshot applied after bone transforms finalize", "rifle_geometry": list(unreal.DuelAssetLibrary.describe_rifle_geometry())}, stream, indent=2)
unreal.log("TPSDuel: combat pose installed")
