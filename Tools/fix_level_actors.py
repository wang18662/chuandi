# 在**编辑器里**执行（Tools → Execute Python Script…），解决关卡保存报错：
#   Illegal reference to private object: MaterialInstanceEditorOnlyData ...
# 原因：旧版代码在构造函数里创建了 Dynamic Material，它被序列化进了关卡里的 Boss Actor。
# 代码已修（MID 改到 BeginPlay 生成），但**已放置的旧 Actor 里仍残留那个子对象**，
# 所以要把这类 Actor 删除后重新生成一次，再保存关卡。

import unreal

EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.EditorLevelLibrary.get_editor_world()

CLASSES = [unreal.ChuandiBossCharacter, unreal.ChuandiPlayerCharacter]
report = []

for cls in CLASSES:
    for a in list(unreal.GameplayStatics.get_all_actors_of_class(world, cls)):
        tr = a.get_actor_transform()
        label = a.get_actor_label()
        a.destroy_actor()
        fresh = EAS.spawn_actor_from_class(cls, tr.translation, tr.rotation.rotator())
        if fresh:
            fresh.set_actor_label(label)
            fresh.set_actor_scale3d(tr.scale3d)
            report.append("respawned " + label)

ok = unreal.EditorLevelLibrary.save_current_level()
unreal.log("FIX_RESULT: " + ", ".join(report) + " | saved=" + str(ok))
