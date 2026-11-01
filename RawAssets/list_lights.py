import unreal
LOG=[]
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
LES.load_level("/Game/Chuandi/Maps/L_Arena")
w = unreal.EditorLevelLibrary.get_editor_world()
names = ["DirectionalLight","SkyLight","PostProcessVolume","ExponentialHeightFog","SkyAtmosphere","VolumetricCloud","StaticMeshActor"]
for n in names:
    try:
        cls = getattr(unreal, n)
        for a in unreal.GameplayStatics.get_all_actors_of_class(w, cls):
            LOG.append("%s: %s" % (n, a.get_actor_label()))
    except Exception as ex:
        LOG.append(n + " ERR " + str(ex))
open(r"D:/UnrealProjects/Chuandi/RawAssets/list_lights.txt","w",encoding="utf-8").write("\n".join(LOG))
