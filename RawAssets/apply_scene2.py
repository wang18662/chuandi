import unreal

LOG = []
def log(*a): LOG.append(" ".join(str(x) for x in a))

LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
MAP = "/Game/Chuandi/Maps/L_Arena"
MAP_FILE = "/Game/Chuandi/Maps/L_Arena"

if not LES.load_level(MAP):
    log("LOAD_FAILED")
else:
    world = unreal.EditorLevelLibrary.get_editor_world()
    lava = unreal.load_asset("/Game/Chuandi/Materials/M_Lava")
    sky_mat = unreal.load_asset("/Game/Chuandi/Materials/M_VolcanicSky")

    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor):
        label = a.get_actor_label()
        comp = a.static_mesh_component
        if label == "Floor":
            comp.set_material(0, lava)
            log("FLOOR_MAT_LAVA")
        elif "SkySphere" in label or "SkyDome" in label:
            comp.set_material(0, sky_mat)
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            comp.set_cast_shadow(False)
            log("SKY_MAT_VOLCANIC", label)
        else:
            # 之前误建的空天球，删掉
            if label == "SkyDome_Volcanic":
                a.destroy_actor()
                log("REMOVED_DUPLICATE_SKY")

    ok = unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_FILE)
    log("SAVE_RESULT", ok)

open(r"D:/UnrealProjects/Chuandi/RawAssets/map_apply_log2.txt", "w", encoding="utf-8").write("\n".join(LOG))
