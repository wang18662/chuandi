import unreal
LOG=[]
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
LES.load_level("/Game/Chuandi/Maps/L_Arena")
w = unreal.EditorLevelLibrary.get_editor_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.StaticMeshActor):
    try:
        c = a.static_mesh_component
        mesh = c.static_mesh
        m = c.get_material(0)
        LOG.append("%s | mesh=%s | mat=%s | scale=%s | loc=%s" % (
            a.get_actor_label(),
            mesh.get_name() if mesh else "None",
            m.get_name() if m else "None",
            str(a.get_actor_scale3d()),
            str(a.get_actor_location())))
    except Exception as ex:
        LOG.append("ERR " + str(ex))
open(r"D:/UnrealProjects/Chuandi/RawAssets/list_sky.txt","w",encoding="utf-8").write("\n".join(LOG))
