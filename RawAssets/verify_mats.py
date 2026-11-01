import unreal
MEL = unreal.MaterialEditingLibrary
LOG=[]
for name in ["M_Lava", "M_VolcanicSky"]:
    try:
        m = unreal.load_asset("/Game/Chuandi/Materials/" + name)
        exprs = MEL.get_material_expressions(m)
        LOG.append("%s: expr=%d shading=%s two_sided=%s" % (name, len(exprs),
            m.get_editor_property("shading_model"), m.get_editor_property("two_sided")))
        for e in exprs:
            tex = ""
            try:
                t = e.get_editor_property("texture")
                if t: tex = t.get_name()
            except Exception:
                pass
            LOG.append("   - %s %s" % (e.get_class().get_name(), tex))
    except Exception as ex:
        LOG.append("%s FAILED: %s" % (name, ex))
open(r"D:/UnrealProjects/Chuandi/RawAssets/verify_mats.txt","w",encoding="utf-8").write("\n".join(LOG))
