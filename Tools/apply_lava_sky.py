# 在**编辑器里**执行（Tools → Execute Python Script…）。
# 作用：把地板换成熔岩材质、天空球换成火山天空材质，并保存关卡。
# 为什么不用命令行：UE 的 commandlet 里没有编辑器世界，save_map 会直接返回 False。

import unreal

world = unreal.EditorLevelLibrary.get_editor_world()
lava = unreal.load_asset("/Game/Chuandi/Materials/M_Lava")
sky  = unreal.load_asset("/Game/Chuandi/Materials/M_VolcanicSky")

changed = []
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor):
    label = a.get_actor_label()
    comp = a.static_mesh_component
    if label == "Floor":
        comp.set_material(0, lava)
        changed.append("Floor -> M_Lava")
    elif "SkySphere" in label or "SkyDome" in label:
        comp.set_material(0, sky)
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_cast_shadow(False)
        changed.append(label + " -> M_VolcanicSky")

ok = unreal.EditorLevelLibrary.save_current_level()
unreal.log("APPLIED: " + ", ".join(changed) + " | saved=" + str(ok))
