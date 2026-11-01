import unreal
MEL = unreal.MaterialEditingLibrary
m = unreal.load_asset("/Game/Chuandi/Materials/M_VolcanicSky")
LOG = ["functions: " + ", ".join(x for x in dir(MEL) if "property" in x.lower())]
# 尝试读取各属性输入
for prop, name in [(unreal.MaterialProperty.MP_EMISSIVE_COLOR, "EMISSIVE"), (unreal.MaterialProperty.MP_BASE_COLOR, "BASE")]:
    try:
        n = MEL.get_material_property_input_node(m, prop)
        LOG.append("%s input node: %s" % (name, n.get_name() if n else "None"))
    except Exception as ex:
        LOG.append("%s ERR %s" % (name, ex))
# 贴图信息
t = unreal.load_asset("/Game/Chuandi/Textures/Sky/T_VolcanicSky_2K")
LOG.append("tex srgb=%s compression=%s" % (t.get_editor_property("srgb"), t.get_editor_property("compression_settings")))
open(r"D:/UnrealProjects/Chuandi/RawAssets/probe_sky.txt","w",encoding="utf-8").write("\n".join(LOG))
