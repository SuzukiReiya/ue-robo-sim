"""UE 5.6で高精細PBR材質を自動生成する。旧256px材質は触らない。"""
import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from GenerateTextures import MAPS, SURFACES, generate_all

ASSET_SUFFIX = "PBRv2"
DIRECTORY = "/Game/Generated"


def exists(path):
    return unreal.EditorAssetLibrary.does_asset_exist(path)


def require_connection(result, what):
    if not result:
        raise RuntimeError("材質ノードを接続できません: " + what)


def import_texture(surface, kind, filename):
    asset_name = f"T_{surface}_{kind}_{ASSET_SUFFIX}"
    path = DIRECTORY + "/" + asset_name
    if exists(path):
        texture = unreal.EditorAssetLibrary.load_asset(path)
    else:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(filename))
        task.set_editor_property("destination_path", DIRECTORY)
        task.set_editor_property("destination_name", asset_name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", True)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.EditorAssetLibrary.load_asset(path)
    if texture is None:
        raise RuntimeError("テクスチャの読み込みに失敗しました: " + path)
    # Normal/Roughnessは色ではなく物理量。sRGB変換すると法線が歪む。
    texture.set_editor_property("srgb", kind == "BaseColor")
    if kind == "Normal":
        texture.set_editor_property(
            "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    elif kind == "Roughness":
        texture.set_editor_property(
            "compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError("テクスチャを保存できません: " + path)
    return texture


def new_expression(material, cls, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(
        material, cls, x, y)


def create_robot_material():
    """画像追尾用の赤い対象は、従来どおり照明に依存しない単色。"""
    path = DIRECTORY + "/M_Robot"
    if exists(path):
        return
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_Robot", DIRECTORY, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError("ロボット材質を生成できませんでした")
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    tint = new_expression(material, unreal.MaterialExpressionVectorParameter, -400, 0)
    tint.set_editor_property("parameter_name", "Color")
    tint.set_editor_property("default_value", unreal.LinearColor(0.6, 0.6, 0.6, 1))
    require_connection(unreal.MaterialEditingLibrary.connect_material_property(
        tint, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "ロボット発光色")
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("ロボット材質を保存できません")


def create_pbr_material(surface, textures):
    """Default LitへBase Color・Normal・Roughnessを接続する。"""
    name = f"M_{surface}_{ASSET_SUFFIX}"
    path = DIRECTORY + "/" + name
    if exists(path):
        return
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, DIRECTORY, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError("PBR材質を生成できません: " + name)
    material.set_editor_property(
        "shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("two_sided", True)

    tint = new_expression(material, unreal.MaterialExpressionVectorParameter, -800, -300)
    tint.set_editor_property("parameter_name", "Color")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    uv = new_expression(material, unreal.MaterialExpressionTextureCoordinate, -1050, 180)
    # 同じUVを3画像で使用し、色・凹凸・粗さがずれないようにする。
    uv.set_editor_property("u_tiling", 4.0 if surface == "Corrugated" else 3.0)
    uv.set_editor_property("v_tiling", 2.0 if surface == "Corrugated" else 3.0)

    samples = {}
    for i, kind in enumerate(MAPS):
        sample = new_expression(
            material, unreal.MaterialExpressionTextureSample, -700, i * 260 - 70)
        sample.set_editor_property("texture", textures[kind])
        if kind == "Normal":
            sample.set_editor_property(
                "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        elif kind == "Roughness":
            sample.set_editor_property(
                "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        else:
            sample.set_editor_property(
                "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        require_connection(unreal.MaterialEditingLibrary.connect_material_expressions(
            uv, "", sample, "UVs"), kind + " UV")
        samples[kind] = sample

    tinted = new_expression(material, unreal.MaterialExpressionMultiply, -360, -100)
    require_connection(unreal.MaterialEditingLibrary.connect_material_expressions(
        samples["BaseColor"], "RGB", tinted, "A"), "Base Color画像")
    require_connection(unreal.MaterialEditingLibrary.connect_material_expressions(
        tint, "", tinted, "B"), "色補正")
    require_connection(unreal.MaterialEditingLibrary.connect_material_property(
        tinted, "", unreal.MaterialProperty.MP_BASE_COLOR), "Base Color出力")
    require_connection(unreal.MaterialEditingLibrary.connect_material_property(
        samples["Normal"], "RGB", unreal.MaterialProperty.MP_NORMAL), "Normal出力")
    require_connection(unreal.MaterialEditingLibrary.connect_material_property(
        samples["Roughness"], "R", unreal.MaterialProperty.MP_ROUGHNESS),
        "Roughness出力")

    # 屋内でLumenを無効化した場合でも真っ黒にならない弱い環境光補助。
    fill = new_expression(material, unreal.MaterialExpressionMultiply, -100, -300)
    intensity = new_expression(
        material, unreal.MaterialExpressionConstant, -340, -480)
    intensity.set_editor_property("r", 0.08)
    require_connection(unreal.MaterialEditingLibrary.connect_material_expressions(
        tinted, "", fill, "A"), "暗部補助色")
    require_connection(unreal.MaterialEditingLibrary.connect_material_expressions(
        intensity, "", fill, "B"), "暗部補助強度")
    require_connection(unreal.MaterialEditingLibrary.connect_material_property(
        fill, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "暗部補助出力")
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("PBR材質を保存できません: " + name)


unreal.EditorAssetLibrary.make_directory(DIRECTORY)
create_robot_material()
# 完成済みのUEアセットがある場合は、PNG生成・取り込みを省略する。
for surface in SURFACES:
    material_path = DIRECTORY + f"/M_{surface}_{ASSET_SUFFIX}"
    if exists(material_path):
        continue
    files = generate_all(Path(unreal.Paths.project_saved_dir()) / "GeneratedTextures")
    maps = {kind: import_texture(surface, kind, files[surface][kind])
            for kind in MAPS}
    create_pbr_material(surface, maps)

map_path = DIRECTORY + "/Minimal"
if not exists(map_path):
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, map_path):
        raise RuntimeError("マップを保存できませんでした")
unreal.log("ROBOT_SIM_MAP_READY: PBR v2")
