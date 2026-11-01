import unreal
LOG=[]
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
LES.load_level("/Game/Chuandi/Maps/L_Arena")
w = unreal.EditorLevelLibrary.get_editor_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.ChuandiSkyDome):
    comps = a.get_components_by_class(unreal.StaticMeshComponent)
    for c in comps:
        LOG.append("dome mesh=%s mat=%s scale=%s" % (c.static_mesh.get_name(), c.get_material(0).get_name(), a.get_actor_scale3d()))
open(r"D:/UnrealProjects/Chuandi/RawAssets/verify_sky2.txt","w",encoding="utf-8").write("\n".join(LOG))
