"""AP5と同じ方式で、空マップと初期材質をUE内で生成する。"""
import unreal

unreal.EditorAssetLibrary.make_directory('/Game/Generated')
path = '/Game/Generated/M_Robot'
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_Robot', '/Game/Generated', unreal.Material, unreal.MaterialFactoryNew())
    color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter)
    color.set_editor_property('parameter_name', 'Color')
    color.set_editor_property('default_value', unreal.LinearColor(0.5, 0.5, 0.5, 1))
    unreal.MaterialEditingLibrary.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError('材質を保存できませんでした')
map_path = '/Game/Generated/Minimal'
if not unreal.EditorAssetLibrary.does_asset_exist(map_path):
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, map_path):
        raise RuntimeError('マップを保存できませんでした')
unreal.log('ROBOT_SIM_MAP_READY')
