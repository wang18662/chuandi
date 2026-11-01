import unreal
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
LES.load_level("/Game/Chuandi/Maps/L_Arena")
w = unreal.EditorLevelLibrary.get_editor_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PostProcessVolume):
    s = a.get_editor_property("settings")
    names = [x for x in dir(s) if "exposure" in x.lower() or "override" in x.lower()]
    open(r"D:/UnrealProjects/Chuandi/RawAssets/probe_ppv.txt","w",encoding="utf-8").write("\n".join(names))
    break
