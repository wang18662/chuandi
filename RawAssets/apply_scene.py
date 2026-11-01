import unreal

LOG = []
def log(*a):
    LOG.append(" ".join(str(x) for x in a))

EAL = unreal.EditorAssetLibrary
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

MAP = "/Game/Chuandi/Maps/L_Arena"
if not LES.load_level(MAP):
    log("LOAD_FAILED", MAP)
else:
    log("LOADED", MAP)
    world = unreal.EditorLevelLibrary.get_editor_world()
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor)
    log("STATIC_MESH_ACTORS", len(actors))

    lava = unreal.load_asset("/Game/Chuandi/Materials/M_Lava")
    sky_mat = unreal.load_asset("/Game/Chuandi/Materials/M_VolcanicSky")
    sphere_mesh = unreal.load_asset("/Engine/BasicShapes/Sphere.Sphere")

    for a in actors:
        c = a.static_mesh_component
        origin, extent = a.get_actor_bounds(False)
        area = extent.x * extent.y
        log("ACTOR", a.get_actor_label(), "loc=%.0f,%.0f,%.0f" % (origin.x, origin.y, origin.z),
            "ext=%.0f,%.0f,%.0f" % (extent.x, extent.y, extent.z), "area=%.0f" % area)
        # 地板：最扁 + 面积最大的那个
        if extent.z < 200.0 and area > 100000.0:
            c.set_material(0, lava)
            log("  -> applied M_Lava")

    # 天空球（倒扣的大球，双面 Unlit）
    sky = EAS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    sky.set_actor_label("SkyDome_Volcanic")
    comp = sky.static_mesh_component
    comp.set_mobility(unreal.ComponentMobility.STATIC)
    comp.set_static_mesh(sphere_mesh)
    comp.set_material(0, sky_mat)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_cast_shadow(False)
    sky.set_actor_scale3d(unreal.Vector(800, 800, 800))
    log("SPAWNED_SKY")

    ok = LES.save_current_level()
    log("SAVE_RESULT", ok)

open(r"D:/UnrealProjects/Chuandi/RawAssets/map_apply_log.txt", "w", encoding="utf-8").write("\n".join(LOG))
