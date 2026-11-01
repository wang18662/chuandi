import unreal
LOG=[]
def log(*a): LOG.append(" ".join(str(x) for x in a))

LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MAP = "/Game/Chuandi/Maps/L_Arena"
if not LES.load_level(MAP):
    log("LOAD_FAILED")
else:
    world = unreal.EditorLevelLibrary.get_editor_world()
    for cls in [unreal.ChuandiBossCharacter, unreal.ChuandiPlayerCharacter]:
        for a in list(unreal.GameplayStatics.get_all_actors_of_class(world, cls)):
            tr = a.get_actor_transform(); label = a.get_actor_label()
            a.destroy_actor()
            fresh = EAS.spawn_actor_from_class(cls, tr.translation, tr.rotation.rotator())
            if fresh:
                fresh.set_actor_label(label); fresh.set_actor_scale3d(tr.scale3d)
            log("RESPAWN", label, "ok" if fresh else "fail")
    ok = unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    log("SAVE_RESULT", ok)
open(r"D:/UnrealProjects/Chuandi/RawAssets/fix_level_log.txt","w",encoding="utf-8").write("\n".join(LOG))
