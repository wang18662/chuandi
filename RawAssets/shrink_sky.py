import unreal
LOG=[]
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
LES.load_level("/Game/Chuandi/Maps/L_Arena")
w = unreal.EditorLevelLibrary.get_editor_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.StaticMeshActor):
    if a.get_actor_label() == "SM_SkySphere":
        # 基础半径 4096cm × 15 = 614m，小于 1km 远裁剪面；中心放场地中央
        a.set_actor_location(unreal.Vector(5000, 5000, 0), False, False)
        a.set_actor_scale3d(unreal.Vector(15, 15, 15))
        LOG.append("shrunk+centered SM_SkySphere")
ok = unreal.EditorLoadingAndSavingUtils.save_map(w, "/Game/Chuandi/Maps/L_Arena")
LOG.append("SAVE " + str(ok))
open(r"D:/UnrealProjects/Chuandi/RawAssets/shrink_sky.txt","w",encoding="utf-8").write("\n".join(LOG))
