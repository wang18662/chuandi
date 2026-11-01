import unreal
LOG=[]
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LES.load_level("/Game/Chuandi/Maps/L_Arena")
w = unreal.EditorLevelLibrary.get_editor_world()

# 删掉所有旧天空（超大 SM_SkySphere + 可能的重复）
for a in list(unreal.GameplayStatics.get_all_actors_of_class(w, unreal.StaticMeshActor)):
    lbl = a.get_actor_label()
    if "Sky" in lbl or "sky" in lbl:
        a.destroy_actor()
        LOG.append("removed " + lbl)

# 放跟随相机的天空球
dome = EAS.spawn_actor_from_class(unreal.ChuandiSkyDome, unreal.Vector(5000, 5000, 0), unreal.Rotator(0, 0, 0))
LOG.append("spawned ChuandiSkyDome" if dome else "SPAWN_FAIL")

ok = unreal.EditorLoadingAndSavingUtils.save_map(w, "/Game/Chuandi/Maps/L_Arena")
LOG.append("SAVE " + str(ok))
open(r"D:/UnrealProjects/Chuandi/RawAssets/swap_sky.txt","w",encoding="utf-8").write("\n".join(LOG))
