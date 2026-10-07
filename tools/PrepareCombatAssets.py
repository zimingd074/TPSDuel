"""Install the native two-hand weapon controller on the duplicated AnimBP."""
import json
import os
import unreal

project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
report_path = os.path.join(project, "Saved", "CombatAssets.json")
with open(report_path, "w", encoding="utf-8") as stream:
    json.dump({"state": "running"}, stream)
combat_clips = []
stride_report = {}
if unreal.EditorAssetLibrary.does_asset_exist("/Game/AnimStarterPack/Idle_Rifle_Hip"):
    combat_clips = list(unreal.DuelAssetLibrary.retarget_combat_animations())
    if len(combat_clips) != 17:
        raise RuntimeError("Expected 16 retargeted rifle clips and one directional BlendSpace")
    # Synchronize the same foot phase across directional clips. These authored
    # loops contain two strides and their left/right plant phases differ.
    for asset_path in combat_clips:
        asset = unreal.load_asset(asset_path)
        name = asset.get_name()
        if not (name.startswith("ASP_Jog_") or name.startswith("ASP_Walk_")):
            continue
        length = asset.get_editor_property("sequence_length")
        is_jog = name.startswith("ASP_Jog_")
        asset.set_editor_property("rate_scale", 375.0 / 270.0 if is_jog else 187.5 / 150.0)
        library = unreal.AnimationLibrary
        library.remove_all_animation_sync_markers(asset)
        if "DuelFeet" not in [str(track) for track in library.get_animation_notify_track_names(asset)]:
            library.add_animation_notify_track(asset, "DuelFeet")
        markers = []
        axis = "y" if "_Fwd_" in name or "_Bwd_" in name else "x"
        sign = -1 if "_Bwd_" in name or "_Rt_" in name else 1
        for side, marker in (("l", "LeftStride"), ("r", "RightStride")):
            trajectory = []
            for frame in range(128):
                bones = ["foot_" + side, "calf_" + side, "thigh_" + side, "pelvis", "root"]
                poses = library.get_bone_poses_for_time(asset, bones, length * frame / 128.0, False)
                pose = poses[0]
                for parent in poses[1:]:
                    pose = unreal.MathLibrary.compose_transforms(pose, parent)
                trajectory.append(getattr(pose.translation, axis) * sign)
            peaks = [i for i in range(128) if trajectory[i] >= trajectory[(i - 1) % 128] and trajectory[i] > trajectory[(i + 1) % 128]]
            if len(peaks) != 2:
                raise RuntimeError("Expected two strides per foot: " + name + " " + side)
            for frame in peaks:
                time = length * frame / 128.0
                library.add_animation_sync_marker(asset, marker, time, "DuelFeet")
                markers.append({"name": marker, "time": time})
        stride_report[name] = {"rate": asset.get_editor_property("rate_scale"), "markers": markers}
    # Refresh the BlendSpace's marker cache after modifying its sequence samples.
    combat_clips = list(unreal.DuelAssetLibrary.retarget_combat_animations())
    for asset in combat_clips:
        if not unreal.EditorAssetLibrary.save_asset(asset):
            raise RuntimeError("Could not save combat animation: " + asset)
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
    json.dump({"state": "prepared", "weapon_pose": "native two-bone IK, grip fingers, muzzle-down low ready and procedural magazine reload", "locomotion": "ASP directional rifle clips, speed/direction BlendSpace" if combat_clips else "template fallback", "stride_sync": stride_report, "actions": "ASP upper-body fire/reload with synchronized hand and magazine IK", "combat_assets": combat_clips, "pose_sync": "shared animation snapshot applied after bone transforms finalize", "rifle_geometry": list(unreal.DuelAssetLibrary.describe_rifle_geometry())}, stream, indent=2)
unreal.log("TPSDuel: combat pose installed")
