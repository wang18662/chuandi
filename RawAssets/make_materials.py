import unreal

AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

LavaColor  = unreal.load_asset("/Game/Chuandi/Textures/Lava/Lava003_2K-JPG_Color")
LavaNormal = unreal.load_asset("/Game/Chuandi/Textures/Lava/Lava003_2K-JPG_NormalDX")
LavaRough  = unreal.load_asset("/Game/Chuandi/Textures/Lava/Lava003_2K-JPG_Roughness")
LavaEmiss  = unreal.load_asset("/Game/Chuandi/Textures/Lava/Lava003_2K-JPG_Emission")
SkyTex     = unreal.load_asset("/Game/Chuandi/Textures/Sky/T_VolcanicSky_2K")

def fresh(name):
    path = "/Game/Chuandi/Materials/" + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    return AT.create_asset(name, "/Game/Chuandi/Materials", unreal.Material, unreal.MaterialFactoryNew())

# ── M_Lava：UV 平铺 20，颜色/粗糙/法线/自发光 ──
m = fresh("M_Lava")
uv = MEL.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 120)
uv.u_tiling = 200.0
uv.v_tiling = 200.0

def samp(tex, x, y, normal=False):
    e = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSample, x, y)
    e.texture = tex
    if normal:
        e.sampler_type = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    MEL.connect_material_expressions(uv, "", e, "UVs")
    return e

s_col = samp(LavaColor, -600, -320)
MEL.connect_material_property(s_col, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

s_rgh = samp(LavaRough, -600, -60)
MEL.connect_material_property(s_rgh, "R", unreal.MaterialProperty.MP_ROUGHNESS)

s_nrm = samp(LavaNormal, -600, 180, normal=True)
MEL.connect_material_property(s_nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)

s_emi = samp(LavaEmiss, -600, 420)
MEL.connect_material_property(s_emi, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

MEL.recompile_material(m)
EAL.save_asset("/Game/Chuandi/Materials/M_Lava", only_if_is_dirty=False)
print("LAVA_MAT_OK")

# ── M_VolcanicSky：Unlit + 双面，天空贴图直连自发光 ──
sky = fresh("M_VolcanicSky")
sky.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
sky.set_editor_property("two_sided", True)
ts = MEL.create_material_expression(sky, unreal.MaterialExpressionTextureSample, -500, 0)
ts.texture = SkyTex
ts.sampler_type = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
MEL.connect_material_property(ts, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(sky)
EAL.save_asset("/Game/Chuandi/Materials/M_VolcanicSky", only_if_is_dirty=False)
print("SKY_MAT_OK")
