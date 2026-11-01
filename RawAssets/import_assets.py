import unreal, os, glob

AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary

def import_many(files, dest):
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.filename = f
        t.destination_path = dest
        t.automated = True
        t.replace_existing = True
        t.save = True
        tasks.append(t)
    if tasks:
        AT.import_asset_tasks(tasks)
    return [p for t in tasks for p in t.imported_object_paths]

# ── 1) 贴图 ──
lava_raw = r"D:/UnrealProjects/Chuandi/RawAssets/Lava"
lava = ["{}/Lava003_2K-JPG_Color.jpg".format(lava_raw),
        "{}/Lava003_2K-JPG_NormalDX.jpg".format(lava_raw),
        "{}/Lava003_2K-JPG_Roughness.jpg".format(lava_raw),
        "{}/Lava003_2K-JPG_Emission.jpg".format(lava_raw)]
sky_raw = r"D:/UnrealProjects/Chuandi/RawAssets/Sky/T_VolcanicSky_2K.png"

tex_lava = import_many(lava, "/Game/Chuandi/Textures/Lava")
tex_sky  = import_many([sky_raw], "/Game/Chuandi/Textures/Sky")
print("LAVA_TEX:", tex_lava)
print("SKY_TEX:", tex_sky)

# 法线贴图要设成 Normalmap 且关 sRGB
for p in tex_lava:
    if "Normal" in p:
        t = unreal.load_asset(p)
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        t.set_editor_property("srgb", False)
        t.set_editor_property("flip_green_channel", False)   # DirectX 法线，UE 默认就是 DX
        unreal.EditorAssetLibrary.save_asset(p, only_if_is_dirty=False)

# ── 2) 音效（130 个 ogg）──
audio_files = sorted(glob.glob(r"C:/Users/25544/Downloads/kenney_impact-sounds/Audio/*.ogg"))
snd = import_many(audio_files, "/Game/Chuandi/Audio/Impact")
print("AUDIO_COUNT:", len(snd))

# ── 3) 材质：M_Lava ──
LavaColor = unreal.load_asset("/Game/Chuandi/Textures/Lava/Lava003_2K-JPG_Color")
LavaNormal = unreal.load_asset("/Game/Chuandi/Textures/Lava/Lava003_2K-JPG_NormalDX")
LavaRough = unreal.load_asset("/Game/Chuandi/Textures/Lava/Lava003_2K-JPG_Roughness")
LavaEmiss = unreal.load_asset("/Game/Chuandi/Textures/Lava/Lava003_2K-JPG_Emission")
SkyTex = unreal.load_asset("/Game/Chuandi/Textures/Sky/T_VolcanicSky_2K")

def new_mat(name):
    path = "/Game/Chuandi/Materials/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    return AT.create_asset(name, "/Game/Chuandi/Materials", unreal.Material, unreal.MaterialFactoryNew())

m_lava = new_mat("M_Lava")
ts_col = MEL.create_material_expression(m_lava, unreal.MaterialExpressionTextureSample, -600, -260)
ts_col.texture = LavaColor
MEL.connect_material_property(ts_col, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

ts_rough = MEL.create_material_expression(m_lava, unreal.MaterialExpressionTextureSample, -600, 60)
ts_rough.texture = LavaRough
MEL.connect_material_property(ts_rough, "R", unreal.MaterialProperty.MP_ROUGHNESS)

ts_nrm = MEL.create_material_expression(m_lava, unreal.MaterialExpressionTextureSample, -600, 260)
ts_nrm.texture = LavaNormal
ts_nrm.sampler_type = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
MEL.connect_material_property(ts_nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)

ts_emi = MEL.create_material_expression(m_lava, unreal.MaterialExpressionTextureSample, -600, 460)
ts_emi.texture = LavaEmiss
mul = MEL.create_material_expression(m_lava, unreal.MaterialExpressionMultiply, -280, 460)
mul.b = 3.0
MEL.connect_material_expressions(ts_emi, "RGB", mul, "A")
MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

MEL.recompile_material(m_lava)
unreal.EditorAssetLibrary.save_asset("/Game/Chuandi/Materials/M_Lava", only_if_is_dirty=False)

# ── 4) 材质：M_VolcanicSky（Unlit + TwoSided）──
m_sky = new_mat("M_VolcanicSky")
m_sky.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m_sky.set_editor_property("two_sided", True)
ts_sky = MEL.create_material_expression(m_sky, unreal.MaterialExpressionTextureSample, -500, 0)
ts_sky.texture = SkyTex
ts_sky.sampler_type = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
MEL.connect_material_property(ts_sky, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(m_sky)
unreal.EditorAssetLibrary.save_asset("/Game/Chuandi/Materials/M_VolcanicSky", only_if_is_dirty=False)

print("MATERIALS_DONE")
