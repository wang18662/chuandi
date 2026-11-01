import unreal
LOG=[]
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
LES.load_level("/Game/Chuandi/Maps/L_Arena")
world = unreal.EditorLevelLibrary.get_editor_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor):
    comp = a.static_mesh_component
    m = comp.get_material(0) if comp else None
    LOG.append("SM  %-18s mat=%s" % (a.get_actor_label(), m.get_name() if m else "None"))
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.ChuandiBossCharacter):
    n = 0
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        n += 1
    LOG.append("BOSS %s  geo_parts=%d" % (a.get_actor_label(), n))
open(r"D:/UnrealProjects/Chuandi/RawAssets/verify_level.txt","w",encoding="utf-8").write("\n".join(LOG))
