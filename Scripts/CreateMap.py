"""UE 5.6用のマップ・テクスチャ・材質を生成する。"""
import sys
from pathlib import Path
import unreal

# UE内蔵Pythonで実行。外部pipライブラリを使用しない。
sys.path.insert(0, str(Path(__file__).resolve().parent))
from GenerateTextures import generate_all


def import_texture(name, filename):
    asset_path = '/Game/Generated/T_' + name
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return unreal.EditorAssetLibrary.load_asset(asset_path)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(filename))
    task.set_editor_property('destination_path', '/Game/Generated')
    task.set_editor_property('destination_name', 'T_' + name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', False)
    task.set_editor_property('save', True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if texture is None:
        raise RuntimeError('テクスチャの取り込みに失敗: ' + asset_path)
    return texture


def create_material(name, texture=None):
    path = '/Game/Generated/M_' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.EditorAssetLibrary.load_asset(path)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_' + name, '/Game/Generated', unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError('材質生成に失敗: ' + name)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('two_sided', True)

    tint = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter)
    tint.set_editor_property('parameter_name', 'Color')
    tint.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    if texture is None:
        output = tint
        pin = ''
    else:
        uv = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionTextureCoordinate)
        uv.set_editor_property('u_tiling', 6.0)
        uv.set_editor_property('v_tiling', 3.0)
        sample = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionTextureSample)
        sample.set_editor_property('texture', texture)
        unreal.MaterialEditingLibrary.connect_material_expressions(uv, '', sample, 'UVs')
        output = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionMultiply)
        unreal.MaterialEditingLibrary.connect_material_expressions(sample, 'RGB', output, 'A')
        unreal.MaterialEditingLibrary.connect_material_expressions(tint, '', output, 'B')
        pin = ''
    if not unreal.MaterialEditingLibrary.connect_material_property(
            output, pin, unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('発光色の接続に失敗: ' + name)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError('材質を保存できませんでした: ' + name)
    return material


unreal.EditorAssetLibrary.make_directory('/Game/Generated')
textures = generate_all(Path(unreal.Paths.project_saved_dir()) / 'GeneratedTextures')
create_material('Robot')

for name, file_path in textures.items():
    create_material(name, import_texture(name, file_path))

map_path = '/Game/Generated/Minimal'
if not unreal.EditorAssetLibrary.does_asset_exist(map_path):
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, map_path):
        raise RuntimeError('マップを保存できませんでした')
unreal.log('ROBOT_SIM_MAP_READY')
