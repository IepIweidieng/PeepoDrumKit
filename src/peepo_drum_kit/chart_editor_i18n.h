#pragma once
#include "core_types.h"
#include "imgui/imgui_include.h"

#define PEEPODRUMKIT_UI_STRINGS_X_MACRO_LIST \
X("Game Preview",						u8"ゲームプレビュー",							u8"游戏预览", ) \
X("Chart Timeline",						u8"タイムライン",								u8"谱面时间线", ) \
X("Chart Timeline - Debug",				u8"タイムライン - デバッグ",					u8"谱面时间线 - 调试", ) \
X("Chart Properties",					u8"譜面プロパティ",							u8"铺面属性", ) \
X("Chart Tempo",						u8"譜面リズム",								u8"铺面 BPM", ) \
X("Chart Lyrics",						u8"譜面歌詞",									u8"铺面歌词", ) \
X("Tempo Calculator",					u8"テンポ計算機",								u8"速度计算器", ) \
X("Undo History",						u8"ヒストリー",								u8"撤销历史", ) \
X("Chart Inspector",					u8"インスペクター",							u8"谱面检查器", ) \
X("Settings",							u8"環境設定",									u8"环境设置", ) \
X("Usage Guide",						u8"使い方について",							u8"使用教程", ) \
X("Update Notes",						u8"更新の履歴",								u8"更新记录", ) \
X("TJA Export Debug View",				u8"TJAエクスポートテスト",						u8"TJA 导出调试视图", ) \
X("TJA Import Test",					u8"TJAインポートテスト",						u8"TJA 导入测试", ) \
X("Audio Test",							u8"オーディオテスト",							u8"音频测试", ) \
X("File",								u8"ファイル",									u8"文件", ) \
X("Edit",								u8"編集",									u8"编辑", ) \
X("Selection",							u8"選択",									u8"选择", ) \
X("Transform",							u8"変換",									u8"变换", ) \
X("Window",								u8"ウィンドウ",								u8"窗口", ) \
X("Language",							u8"言語",									u8"语言", ) \
X("English",							u8"英語",									u8"英语", ) \
X("Japanese",							u8"日本語",									u8"日语", ) \
X("Simplified Chinese",					u8"中国語（簡体字）",							u8"简体中文", ) \
X("%s (%s)",							u8"%s（%s）",								u8"%s（%s）", ) \
X("Help",								u8"ヘルプ",									u8"帮助", ) \
X("Copy",								u8"コピー",									u8"复制", ) \
X("Delete",								u8"削除",									u8"删除", ) \
X("Save",								u8"保存",									u8"保存", ) \
X("Undo",								u8"元に戻す",									u8"撤销", ) \
X("Redo",								u8"やり直し",									u8"重做", ) \
X("Cut",								u8"切り取り",									u8"剪切", ) \
X("Paste",								u8"貼り付け",									u8"粘贴", ) \
X("Open Recent",						u8"最近使用した項目を開く",						u8"打开最近项目", ) \
X("Exit",								u8"終了",									u8"退出", ) \
X("New Chart",							u8"新規ファイル",								u8"新建谱面", ) \
X("Open...",							u8"ファイルを開く...",							u8"打开...", ) \
X("Clear Items",						u8"最近使ったものをクリア",						u8"清除项目", ) \
X("Open Chart Directory...",			u8"エクスプローラーで表示する...",				u8"打开谱面文件夹", ) \
X("Save As...",							u8"名前を付けて保存...",						u8"另存为...", ) \
X("Refine Selection",					u8"選択を絞り込み",							u8"优化所选", ) \
X("Select All",							u8"すべて選択",								u8"全选", ) \
X("Clear Selection",					u8"選択を解除",								u8"取消选择", ) \
X("Invert Selection",					u8"選択を反転",								u8"反选", ) \
X("Start Range Selection",				u8"範囲選択を開始",							u8"范围选择起点", ) \
X("End Range Selection",				u8"範囲選択を終了",							u8"范围选择终点", ) \
X("From Range Selection",				u8"範囲選択以内項目を選択",						u8"范围内的内容", ) \
X("Shift selection Left",				u8"選択を左に移動",							u8"左移范围", ) \
X("Shift selection Right",				u8"選択を右に移動",							u8"右移范围", ) \
X("Select Item Pattern xo",				u8"パターン選択 xo",							u8"选择 xo 音符模板", ) \
X("Select Item Pattern xoo",			u8"パターン選択 xoo",							u8"选择 xoo 音符模板", ) \
X("Select Item Pattern xooo",			u8"パターン選択 xooo",						u8"选择 xooo 音符模板", ) \
X("Select Item Pattern xxoo",			u8"パターン選択 xxoo",						u8"选择 xxoo 音符模板", ) \
X("Add New Pattern...",					u8"新規パターンを追加...",						u8"增加新的音符模板", ) \
X("Select Custom Pattern",				u8"カスタムパターン",							u8"选择自定义模板", ) \
X("Delete?",							u8"削除？",									u8"是否删除？", ) \
X("Flip Note Types",					u8"音符の種類を変化",							u8"翻转音符类型", ) \
X("Toggle Note Sizes",					u8"音符の大小を変化",							u8"切换音符大小", ) \
X("Expand Items",						u8"選択した項目を広げ",						u8"扩展音符", ) \
X("Compress Items",						u8"選択した項目を縮め",						u8"压缩音符", ) \
X("2:1 (8th to 4th)",					u8"2:1 (8分 → 4分)",							u8"2:1 (8分 → 4分)", ) \
X("3:2 (12th to 8th)",					u8"3:2 (12分 → 8分)",						u8"3:2 (12分 → 8分)", ) \
X("4:3 (16th to 12th)",					u8"4:3 (16分 → 12分)",						u8"4:3 (16分 → 12分)", ) \
X("1:2 (4th to 8th)",					u8"1:2 (4分 → 8分)",							u8"1:2 (4分 → 8分)", ) \
X("2:3 (8th to 12th)",					u8"2:3 (8分 → 12分)",						u8"2:3 (8分 → 12分)", ) \
X("3:4 (12th to 16th)",					u8"3:4 (12分 → 16分)",						u8"3:4 (12分 → 16分)", ) \
X("Toggle VSync",						u8"垂直同期",									u8"垂直同步", ) \
X("Toggle Fullscreen",					u8"全画面表示",								u8"全屏显示", ) \
X("Window Size",						u8"ウィンドウサイズ",							u8"窗口大小", ) \
X("Resize to",							u8"リサイズ",									u8"重设大小到", ) \
X("Current Size",						u8"現在のサイズ",								u8"当前大小", ) \
X("DPI Scale",							u8"解像度スケール",							u8"DPI 缩放", ) \
X("Zoom In",							u8"拡大",									u8"放大", ) \
X("Zoom Out",							u8"縮小",									u8"缩小", ) \
X("Reset Zoom",							u8"ズームをリセット",							u8"重置缩放", ) \
X(" Reset ",							u8" リセット ",								u8" 重置 ", ) \
X("Current Scale",						u8"現在のスケール",							u8"当前缩放", ) \
X("Test Menu",							u8"テスト",									u8"测试菜单", ) \
X("Show Audio Test",					u8"オーディオテスト表示",						u8"显示音频测试", ) \
X("Show TJA Import Test",				u8"TJAインポートテスト表示",					u8"显示 TJA 导入测试", ) \
X("Show TJA Export View",				u8"TJAエクスポートテスト表示",					u8"显示 TJA 导出视图", ) \
X("Show ImGui Demo",					u8"ImGui Demo表示",							u8"显示 ImGui Demo", ) \
X("Show ImGui Style Editor",			u8"ImGui Style Editor表示",					u8"显示 ImGui 样式编辑器", ) \
X("Reset Style Colors",					u8"Style Colorsをリセット",					u8"重置样式颜色", ) \
X("Copyright (c) 2022",					u8"Copyright (c) 2022",						u8"Copyright (c) 2022", ) \
X("Build Time:",						u8"ビルド時間:",								u8"构建时间：", ) \
X("Build Date:",						u8"ビルド日付:",								u8"构建日期：", ) \
X("Build Configuration:",				u8"ビルド構成:",								u8"构建配置：", ) \
X("Current Version:",					u8"ビルドバージョン:",							u8"构建版本：", ) \
X("Debug",								u8"デバッグ",									u8"调试", ) \
X("Release",							u8"リリース",									u8"发行", ) \
X("Courses",							u8"コース",									u8"难度", ) \
X("Add New",							u8"追加",									u8"新建", ) \
X("Edit...",							u8"編集...",									u8"编辑...", ) \
X("Open Audio Device",					u8"オーディオデバイスを開く",					u8"打开音频设备", ) \
X("Close Audio Device",					u8"オーディオデバイスを閉じる",					u8"关闭音频设备", ) \
X("Average: ",							u8"平均値: ",								u8"平均值：", ) \
X("Min: ",								u8"最小値: ",								u8"最小值：", ) \
X("Max: ",								u8"最大値: ",								u8"最大值：", ) \
X("Use %s",								u8"%sを使用",								u8"已使用 %s", ) \
X("Peepo Drum Kit - Unsaved Changes",	u8"Peepo Drum Kit - 保存されていない変更",		u8"Peepo Drum Kit - 更改未保存", ) \
X("Save changes to the current file?",	u8"このファイルの変更内容を保存しますか？",		u8"是否保存当前文件的更改？", ) \
X("Save Changes",						u8"保存",									u8"保存", ) \
X("Discard Changes",					u8"保存しない",								u8"放弃保存", ) \
X("Cancel",								u8"キャンセル",								u8"取消", ) \
X("Tempo",								u8"テンポ",									u8"BPM", ) \
X("Time Signature",						u8"拍子記号",									u8"节拍", ) \
X("Notes",								u8"音符",									u8"音符", ) \
X("Notes (Expert)",						u8"音符 (玄人)",								u8"音符（进阶）", ) \
X("Notes (Master)",						u8"音符 (達人)",								u8"音符（达人）", ) \
X("Scroll Speed",						u8"スクロールスピード",						u8"滚动速度", ) \
X("Vertical Scroll Speed",				u8"縦スクロールスピード",						u8"垂直滚动速度", ) \
X("Scroll Speed Tempo",					u8"スクロールスピードのテンポ",					u8"滚动速度 BPM", ) \
X("Bar Line Visibility",				u8"小節線の表示",								u8"小节线可见性", ) \
X("Go-Go Time",							u8"ゴーゴータイム",							u8"Go-Go 区间", ) \
X("Lyrics",								u8"歌詞",									u8"歌词", ) \
X("Scroll Type",						u8"スクロールタイプ",							u8"滚动方式", ) \
X("JPOS Scroll",						u8"判定エリアスクロール",						u8"判定区移动", ) \
X("JPOS Scroll Move",					u8"判定エリア移動",							u8"判定区移动距离", ) \
X("Vertical JPOS Scroll Move",			u8"縦判定エリア移動",							u8"判定区垂直移动距离", ) \
X("JPOS Scroll Duration",				u8"判定エリア移動時間",						u8"判定区移动时间", ) \
X("Sync",								u8"タイミング",								u8"同步", ) \
X("Chart Duration",						u8"譜面の終了時間",							u8"谱面长度", ) \
X("Song Demo Start",					u8"音源のプレビュー時間",						u8"音乐演示开始位置", ) \
X("Song Offset",						u8"音源の再生開始時間",						u8"音乐播放时间位移", ) \
X("Selection to Scroll Changes",		u8"選択した項目をスクロールスピードに変化",		u8"更改所选项目的滚动速度", ) \
X("Set Cursor",							u8"カーソルで設定",							u8"设置为光标进度", ) \
X("Add",								u8"追加",									u8"增加", ) \
X("Remove",								u8"削除",									u8"删除", ) \
X("Clear",								u8"クリア",									u8"清除", ) \
X("Set from Range Selection",			u8"範囲選択で追加",							u8"从选取范围设置", ) \
X("Chart",								u8"創作譜面",									u8"谱面信息", ) \
X("Chart Title",						u8"曲のタイトル",								u8"谱面标题", ) \
X("Chart Subtitle",						u8"曲のサブタイトル",							u8"谱面子标题", ) \
X("Chart Creator",						u8"譜面の作者名",								u8"谱面制作者", ) \
X("Song File Name",						u8"音源のファイル名",							u8"歌曲文件名", ) \
X("Jacket File Name",					u8"ジャケットのファイル名",						u8"歌曲封面文件名", ) \
X("Song Volume",						u8"音源の音量",								u8"歌曲音量", ) \
X("Sound Effect Volume",				u8"音色の音量",								u8"音效音量", ) \
X("Selected Course",					u8"現在のコース",								u8"所选难度", ) \
X("Difficulty Type",					u8"難易度",									u8"难度类型", ) \
X("Difficulty Level",					u8"難易度のレベル",							u8"难度星级", ) \
X("Difficulty Level Decimal",			u8"難易度のレベル( 小数 )",					u8"难度星级小数", ) \
X("Course Creator",						u8"コースの作者名",							u8"难度制作者", ) \
X("Selected Items",						u8"選択した項目",								u8"所选项目", ) \
X("( Nothing Selected )",				u8"( 選択なし )",								u8"（ 未选择项目 ）", ) \
X("Selected ",							u8"選択した",									u8"已选中 ", ) \
X("Items",								u8"項目",									u8"音符", ) \
X("Tempos",								u8"テンポ",									u8"BPM", ) \
X("Time Signatures",					u8"拍子記号",									u8"节拍", ) \
X("Scroll Speeds",						u8"スクロールスピード",						u8"滚动速度", ) \
X("Bar Lines",							u8"小節線の表示",								u8"小节线", ) \
X("Go-Go Ranges",						u8"ゴーゴータイム",							u8"Go-Go 区间", ) \
X("Bar Line Visible",					u8"小節線の表示",								u8"显示小节线", ) \
X("Visible",							u8"表示",									u8"显示", ) \
X("Hidden",								u8"非表示",									u8"隐藏", ) \
X("Scroll Types",						u8"スクロールタイプ",							u8"滚动方式", ) \
X("JPOS Scrolls",						u8"判定座標スクロール",						u8"判定区移动", ) \
X("NMSCROLL",							u8"普通",									u8"一般", ) \
X("HBSCROLL",							u8"HBSCROLL",								u8"HBSCROLL", ) \
X("BMSCROLL",							u8"BMSCROLL",								u8"BMSCROLL", ) \
X("Balloon Pop Count",					u8"風船の連打数",								u8"气球连打数", ) \
X("Interpolate: Scroll Speed",			u8"補間: スクロールスピード",					u8"补间：滚动速度", ) \
X("Interpolate: Scroll Speed Tempo",	u8"補間: スクロールスピードのテンポ",			u8"补间：滚动速度 BPM", ) \
X("Interpolate: Vertical Scroll Speed",	u8"補間: 縦スクロールスピード",					u8"补间：垂直滚动速度", ) \
X("Time Offset",						u8"時間のオフセット",							u8"时间位移", ) \
X("Note Type",							u8"音符の種類",								u8"音符类型", ) \
X("Note Type Size",						u8"音符の大小",								u8"音符大小", ) \
X("Don",								u8"ドン",									u8"咚", ) \
X("DON",								u8"ドン (大)",								u8"咚（大）", ) \
X("Ka",									u8"カッ",									u8"咔", ) \
X("KA",									u8"カッ (大)",								u8"咔（大）", ) \
X("Drumroll",							u8"連打",									u8"连打", ) \
X("DRUMROLL",							u8"連打 (大)",								u8"连打（大）", ) \
X("Balloon",							u8"風船",									u8"气球", ) \
X("BALLOON",							u8"風船 (大)",								u8"气球（大）", ) \
X("Small",								u8"小",										u8"小", ) \
X("Big",								u8"大",										u8"大", ) \
X("Easy",								u8"かんたん",									u8"简单", ) \
X("Normal",								u8"ふつう",									u8"普通", ) \
X("Hard",								u8"むずかしい",								u8"困难", ) \
X("Oni",								u8"おに",									u8"魔王", ) \
X("Oni-Ura",							u8"おに (裏)",								u8"魔王（里）", ) \
X("Single",								u8"一人",									u8"单人", ) \
X("Double",								u8"二人",									u8"双人", ) \
X("Description",						u8"記述",									u8"简介", ) \
X("Time",								u8"時間",									u8"时间", ) \
X("Initial State",						u8"初期状態",									u8"初始状态", ) \
X("Lyrics Overview",					u8"歌詞の概要",								u8"歌词概览", ) \
X("Edit Line",							u8"編集",									u8"编辑该行", ) \
X("(No Lyrics)",						u8"(歌詞なし)",								u8"（没有歌词）", ) \
X("Reset",								u8"リセット",									u8"重置", ) \
X("Tap",								u8"叩いて",									u8"点击", ) \
X(" First Beat ",						u8"一拍",									u8" 第一节拍 ", ) \
X("Nearest Whole",						u8"平均（整数部）",							u8"平均整数 BPM", ) \
X("Nearest",							u8"平均",									u8"平均 BPM", ) \
X("Min and Max",						u8"最小と最大",								u8"最小和最大值", ) \
X("Timing Taps",						u8"叩いた数",									u8"节拍数量", ) \
X("First Beat",							u8"最初の拍",									u8"第一拍", ) \
X("%d Taps",							u8"%d 拍",									u8"%d 拍", ) \
X("",									u8"",										u8"", ) \
X("KADON",								u8"カドン",									u8"咔咚", ) \
X("Bomb",								u8"爆弾",									u8"炸弹", ) \
X("Adlib",								u8"アドリブ",									u8"Adlib", ) \
X("Fuseroll",							u8"時爆弾",									u8"计时弹", ) \
X("Load Full CJKV Glyphs (slow)",		u8"全てのCJKV字形を読み込む（遅い）",			u8"载入所有 CJKV 字形（慢）", ) \

#define UI_Str(in) i18n::HashToString(i18n::CompileTimeValidate<i18n::Hash(in)>(), SelectedGuiLanguage)
#define UI_StrRuntime(in) i18n::HashToString(i18n::Hash(in), SelectedGuiLanguage)
#define UI_WindowName(in) i18n::ToStableName(in, i18n::CompileTimeValidate<i18n::Hash(in)>(), SelectedGuiLanguage).Data

namespace PeepoDrumKit
{
	enum class GuiLanguage : u8 { EN, JA, ZHCN, Count };
	inline GuiLanguage SelectedGuiLanguage = GuiLanguage::EN;

	constexpr struct { GuiLanguage Language; cstr Code, Name; } GuiLanguageDefs[] =
	{
		{ GuiLanguage::EN, "en", "English", },
		{ GuiLanguage::JA, "ja", "Japanese", },
		{ GuiLanguage::ZHCN, "zh-CN", "Simplified Chinese", },
	};
	static_assert(ArrayCount(GuiLanguageDefs) == EnumCount<GuiLanguage>, "Don't forget to implement proper language code string conversion");

	// Converters; no need to optimize

	inline cstr GuiLanguageToLocaleCode(GuiLanguage lang)
	{
		for (const auto& def : GuiLanguageDefs) {
			if (def.Language == lang)
				return def.Code;
		}
		return GuiLanguageDefs[0].Code;
	}

	inline GuiLanguage LocaleCodeToGuiLanguage(std::string_view str)
	{
		for (const auto& def : GuiLanguageDefs) {
			if (def.Name == str)
				return def.Language;
		}
		return GuiLanguageDefs[0].Language;
	}
}

namespace PeepoDrumKit::i18n
{
	constexpr ImU32 Crc32LookupTable[256] =
	{
		0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
		0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
		0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
		0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
		0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
		0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
		0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
		0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
		0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
		0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
		0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
		0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
		0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
		0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
		0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
		0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D,
	};

	constexpr u32 Crc32(const char* data, size_t dataSize, u32 seed)
	{
		seed = ~seed; u32 crc = seed;
		while (dataSize-- != 0)
			crc = (crc >> 8) ^ Crc32LookupTable[(crc & 0xFF) ^ static_cast<u8>(*data++)];
		return ~crc;
	}

	constexpr u32 Hash(std::string_view data, u32 seed = 0xDEADBEEF) { return Crc32(data.data(), data.size(), seed); }

	constexpr u32 AllValidHashes[] =
	{
#define X(en, ja, zhCn) Hash(en),
			PEEPODRUMKIT_UI_STRINGS_X_MACRO_LIST
#undef X
	};

	constexpr b8 IsValidHash(u32 inHash) { for (u32 it : AllValidHashes) { if (it == inHash) return true; } return false; }

	template <u32 InHash>
	constexpr u32 CompileTimeValidate() { static_assert(IsValidHash(InHash), "Unknown string"); return InHash; }

	constexpr cstr HashToString(u32 inHash, GuiLanguage outLanguage)
	{
		switch (inHash)
		{
#define X(en, ja, zhCn) case ForceConsteval<Hash(en)>: return (outLanguage == GuiLanguage::EN) ? en : (outLanguage == GuiLanguage::ZHCN) ? zhCn : ja;
			PEEPODRUMKIT_UI_STRINGS_X_MACRO_LIST
#undef X
		}

#if PEEPO_DEBUG
		assert(!"Missing string entry"); return nullptr;
#endif
		return "(undefined)";
	}

	struct StableNameBuffer { char Data[128]; };
	inline StableNameBuffer ToStableName(cstr inString, u32 inHash, GuiLanguage outLanguage)
	{
		cstr translatedString = HashToString(inHash, outLanguage);
		StableNameBuffer buffer;

		char* out = &buffer.Data[0];
		*out = '\0';
		while (*translatedString != '\0')
			*out++ = *translatedString++;
		*out++ = '#';
		*out++ = '#';
		*out++ = '#';
		while (*inString != '\0')
			*out++ = *inString++;
		*out = '\0';

		// strcpy_s(out.Data, translatedString);
		// strcat_s(out.Data, "###");
		// strcat_s(out.Data, inString);

		return buffer;
	}
}
