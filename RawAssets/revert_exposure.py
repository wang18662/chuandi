import unreal
LOG=[]
def log(*a): LOG.append(" ".join(str(x) for x in a))
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
LES.load_level("/Game/Chuandi/Maps/L_Arena")
w = unreal.EditorLevelLibrary.get_editor_world()

# 1) 恢复自动曝光
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PostProcessVolume):
    s = a.get_editor_property("settings")
    s.auto_exposure_method = unreal.AutoExposureMethod.AEM_HISTOGRAM
    s.override_auto_exposure_method = False
    s.auto_exposure_bias = 0.0
    s.override_auto_exposure_bias = False
    a.set_editor_property("settings", s)
    log("PPV -> auto exposure (histogram)")

# 2) 太阳加亮，暖色保留
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DirectionalLight):
    c = a.light_component
    c.set_light_color(unreal.LinearColor(1.0, 0.88, 0.72, 1.0))
    c.set_intensity(8.0)
    log("SUN -> warm, intensity 8.0")

# 3) 天光适当（暗天里补环境）
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.SkyLight):
    a.light_component.set_intensity(1.5)
    log("SKYLIGHT -> 1.5")

ok = unreal.EditorLoadingAndSavingUtils.save_map(w, "/Game/Chuandi/Maps/L_Arena")
log("SAVE " + str(ok))
open(r"D:/UnrealProjects/Chuandi/RawAssets/revert_exposure.txt","w",encoding="utf-8").write("\n".join(LOG))
