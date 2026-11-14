from pathlib import Path
import sys

root=Path(__file__).resolve().parents[2]
output=Path(sys.argv[1]); output.mkdir(parents=True,exist_ok=True)
def body(path,signature):
    source=(root/path).read_text(encoding='utf-8-sig')
    start=source.index(signature); begin=source.index('{',start); end=begin+1; depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}'); end+=1
    return source[start:end]
units={
    'mode_level.inc':('src/votv-coop/src/ue_wrap/world/game_mode.cpp','const wchar_t* DefaultLevel('),
    'mode_save_map.inc':('src/votv-coop/src/ue_wrap/engine/save_browser.cpp','bool InitializeNewGameMap('),
    'mode_apply.inc':('src/votv-coop/src/ue_wrap/engine/engine_save.cpp','void ApplyGameModeFromSlot('),
}
for name,(path,signature) in units.items():
    (output/name).write_text(body(path,signature),encoding='utf-8')

# Wiring checks supplement actual helper/controller execution and production compilation.
native=(root/'src/votv-coop/src/ui/host_window_native.cpp').read_text(encoding='utf-8-sig')
fallback=(root/'src/votv-coop/src/ui/host_save_picker.cpp').read_text(encoding='utf-8-sig')
assert 'c.mode = host_game_mode::Value();' in native and 'c.mode = host_game_mode::Value();' in fallback
assert 'g_selectedSlot.empty()' in native and 'g_modeButtons={};' in native
assert 'host_game_mode::Label(g_choice.mode)' in (root/'src/votv-coop/src/ui/host_session_settings.cpp').read_text()
boot=(root/'src/votv-coop/src/harness/world_boot.cpp').read_text()
assert 'b->ph.save.newGame?b->ph.save.mode:-1' in boot
assert 'BeginJoinApply(mode);' in boot and 'BeginJoinApply(freshMode);' in boot
join=(root/'src/votv-coop/src/coop/session/net_pump.cpp').read_text()
native_case=join.split('case PIS::JoinPlacement::NativeMap:',1)[1].split('break;',1)[0]
assert 'ApplyLocally' not in native_case
load=(root/'src/votv-coop/src/ue_wrap/engine/engine_save.cpp').read_text()
assert load.count('if (!g_gameModeApplied) return false;')==2
assert load.count('L"/Game/maps/tutorial3"')==2
identity=(root/'src/votv-coop/src/ue_wrap/engine/world_identity.cpp').read_text()
assert 'R::NameContains(R::NameOf(world),L"tutorial3")' in identity
save=(root/'src/votv-coop/src/ue_wrap/engine/save_browser.cpp').read_text()
assert save.index('InitializeNewGameMap(save,mode)')<save.index('// SaveGameToSlot(save,')
transfer=(root/'src/votv-coop/src/coop/save/save_transfer.cpp').read_text()
assert 'g_cliGameMode = p.gameMode;' in transfer
print('PASS mode UI, save, boot, playable-world and existing save-transfer wiring')
