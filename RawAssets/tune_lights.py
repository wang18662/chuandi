import unreal
LOG=[]
def log(*a): LOG.append(" ".join(str(x) for x in a))

LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
LES.load_level("/Game/Chuandi/Maps/L_Arena")
w = unreal.EditorLevelLibrary.get_editor_world()

# 1) 手动曝光（关自动）
try:
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PostProcessVolume):
        s = a.get_editor_property("settings")
        s.auto_exposure_method = unreal.AutoExposureMethod.AEM_MANUAL
        s.auto_exposure_bias = 0.5
        s.override_auto_exposure_method = True
        s.override_auto_exposure_bias = True
        a.set_editor_property("settings", s)
        log("PPV", "manual exposure EV=0.5")
except Exception as ex:
    log("PPV_ERR", str(ex))

# 2) SkyLight 强度 + 实时捕获
try:
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.SkyLight):
        c = a.light_component
        c.set_intensity(2.5)
        c.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_CAPTURED_SCENE)
        c.set_editor_property("real_time_capture", True)
        try:
            c.recapture_sky()
        except Exception:
            pass
        log("SKYLIGHT", "intensity=2.5 realtime")
except Exception as ex:
    log("SKY_ERR", str(ex))

# 3) 太阳变暖色
try:
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DirectionalLight):
        c = a.light_component
        c.set_light_color(unreal.LinearColor(1.0, 0.54, 0.31, 1.0))
        c.set_intensity(3.0)
        log("SUN", "warm orange, intensity=3.0")
except Exception as ex:
    log("SUN_ERR", str(ex))

ok = unreal.EditorLoadingAndSavingUtils.save_map(w, "/Game/Chuandi/Maps/L_Arena")
log("SAVE", ok)
open(r"D:/UnrealProjects/Chuandi/RawAssets/tune_lights_log.txt","w",encoding="utf-8").write("\n".join(LOG))
