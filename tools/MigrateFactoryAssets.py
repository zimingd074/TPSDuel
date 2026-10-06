"""Copy only selected Factory meshes and package dependencies, keeping /Game paths."""
import hashlib
import json
import os
import shutil
import unreal

root = "D:/TPSDuel"
with open(os.path.join(root, "Saved", "FactorySelection.json"), encoding="utf-8") as stream:
    selection = json.load(stream)
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous(["/Game"], True)
options = unreal.AssetRegistryDependencyOptions()
options.set_editor_property("include_hard_package_references", True)
options.set_editor_property("include_soft_package_references", True)
pending = [path.split(".")[0] for path in selection.values()]
packages = set()
while pending:
    package = pending.pop()
    if package in packages or not package.startswith("/Game/"):
        continue
    packages.add(package)
    pending.extend(str(name) for name in registry.get_dependencies(package, options))
source = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
target = os.path.join(root, "Content")
files = []
for package in sorted(packages):
    relative = package[len("/Game/"):] + ".uasset"
    src = os.path.join(source, relative)
    dst = os.path.join(target, relative)
    if not os.path.isfile(src):
        raise RuntimeError("Missing package: " + src)
    if os.path.isfile(dst):
        with open(src, "rb") as stream:
            src_hash = hashlib.sha256(stream.read()).hexdigest()
        with open(dst, "rb") as stream:
            if hashlib.sha256(stream.read()).hexdigest() != src_hash:
                raise RuntimeError("Refusing to overwrite different existing asset: " + dst)
    else:
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
    files.append({"package": package, "file": relative, "bytes": os.path.getsize(dst)})
# The pack uses generic /Game/Materials and /Game/Textures paths. Ignore only
# these migrated files, rather than hiding the project's original materials.
ignore_path = os.path.join(root, ".gitignore")
with open(ignore_path, encoding="utf-8-sig") as stream:
    current_ignore = stream.read()
new_patterns = ["/Content/" + entry["file"].replace("\\", "/") for entry in files]
with open(ignore_path, "a", encoding="utf-8") as stream:
    stream.write("\n# Selected licensed Factory assets and their dependencies.\n")
    for pattern in new_patterns:
        if pattern not in current_ignore.splitlines():
            stream.write(pattern + "\n")
report = {"state": "migrated", "selection": selection, "files": files,
          "bytes": sum(entry["bytes"] for entry in files)}
for report_path in (os.path.join(root, "Saved", "FactoryMigration.json"),
                    os.path.join(os.path.dirname(source.rstrip("/\\")), "TPSDuelMigration.json")):
    with open(report_path, "w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2)
unreal.log("TPSDuel: selected Factory packages migrated: {}".format(len(files)))
