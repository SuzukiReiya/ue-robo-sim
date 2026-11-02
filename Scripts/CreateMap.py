"""空マップと視認性の高い検証用材質を生成・更新する。"""
import unreal

unreal.EditorAssetLibrary.make_directory('/Game/Generated')
path = '/Game/Generated/M_Robot'
if unreal.EditorAssetLibrary.does_asset_exist(path):
    material = unreal.EditorAssetLibrary.load_asset(path)
else:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_Robot', '/Game/Generated', unreal.Material, unreal.MaterialFactoryNew())

if material is None:
    raise RuntimeError('検証用材質を取得できませんでした')

# 旧版のライティング依存材質も更新する。既存アセットがある場合でも毎回チェックする。
# 対象物の色が照明・露出に左右されると画像追跡の基準にできないため、Unlitとする。
if (material.get_editor_property('shading_model') != unreal.MaterialShadingModel.MSM_UNLIT
        or not material.get_editor_property('two_sided')):
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    # カメラは背景用の大きな立方体の内側にある。
    material.set_editor_property('two_sided', True)
    color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter)
    color.set_editor_property('parameter_name', 'Color')
    color.set_editor_property('default_value', unreal.LinearColor(0.5, 0.5, 0.5, 1))
    if not unreal.MaterialEditingLibrary.connect_material_property(
            color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('材質の色パラメータを接続できませんでした')
    errors = unreal.MaterialEditingLibrary.recompile_material(material)
    if errors:
        raise RuntimeError('材質のコンパイルに失敗しました: ' + str(errors))
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError('材質を保存できませんでした')

map_path = '/Game/Generated/Minimal'
if not unreal.EditorAssetLibrary.does_asset_exist(map_path):
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, map_path):
        raise RuntimeError('マップを保存できませんでした')
unreal.log('ROBOT_SIM_MAP_READY')
