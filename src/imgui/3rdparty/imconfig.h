//-----------------------------------------------------------------------------
// DEAR IMGUI COMPILE-TIME OPTIONS
// Runtime options (clipboard callbacks, enabling various features, etc.) can generally be set via the ImGuiIO structure.
// You can use ImGui::SetAllocatorFunctions() before calling ImGui::CreateContext() to rewire memory allocation functions.
//-----------------------------------------------------------------------------
// A) You may edit imconfig.h (and not overwrite it when updating Dear ImGui, or maintain a patch/rebased branch with your modifications to it)
// B) or '#define IMGUI_USER_CONFIG "my_imgui_config.h"' in your project and then add directives in your own file without touching this template.
//-----------------------------------------------------------------------------
// You need to make sure that configuration settings are defined consistently _everywhere_ Dear ImGui is used, which include the imgui*.cpp
// files but also _any_ of your code that uses Dear ImGui. This is because some compile-time options have an affect on data structures.
// Defining those options in imconfig.h will ensure every compilation unit gets to see the same data structure layouts.
// Call IMGUI_CHECKVERSION() from your .cpp file to verify that the data structures your files are using are matching the ones imgui.cpp is using.
//-----------------------------------------------------------------------------

#pragma once
#include "core_types.h"

// HACK: Non pixel aligned text rendering https://github.com/ocornut/imgui/issues/2291
inline b8 IMGUI_HACKS_GLOBAL_DISABLE_FONT_PIXEL_SNAPPING = false;

namespace ImGui
{
	inline void DisableFontPixelSnap(b8 disabled) { IMGUI_HACKS_GLOBAL_DISABLE_FONT_PIXEL_SNAPPING = disabled; }
}

// HACK: Convert the rasterized stb_truetype font alpha values from linear to gamma space as a workaround for not being properly sRGB aware.
//		 The correct gamma value supposedly should be around 1.8 (at least as per the FT_Render_Glyph docs) however that ends up looking "too strong".
//		 For easy testing, a change of this variable will automatically be detected by the backend and force a full font rebuild. (based on https://github.com/ocornut/imgui/pull/4950)
#define IMGUI_HACKS_DELINEARIZE_FONTS 1
inline f32 IMGUI_HACKS_DELINEARIZE_FONTS_GAMMA = 1.3f; // 1.4f; // 1.8f; // 2.2f;

// HACK: Temporary workaround for a potential infinite loop in ImGui::ShrinkWidths() when using unrounded font sizes (https://github.com/ocornut/imgui/issues/5652)
#define IMGUI_HACKS_SHRINK_WIDTHS_LOOP_SAFETY_LIMIT 10'000

// HACK: Because the X buttons aren't defined by default :/
typedef int ImGuiMouseButton;
enum ImGuiMouseButtonEx_ : ImGuiMouseButton
{
	// ImGuiMouseButton_Left = 0,
	// ImGuiMouseButton_Right = 1,
	// ImGuiMouseButton_Middle = 2,
	ImGuiMouseButton_X1 = 3,
	ImGuiMouseButton_X2 = 4,
	// ImGuiMouseButton_COUNT = 5
};

// HACK: Common combinations just to make definding key bindings a little easier
enum ImGuiModEx_
{
	// ImGuiMod_None = 0,
	// ImGuiMod_Ctrl = 1 << 12,
	// ImGuiMod_Shift = 1 << 13,
	// ImGuiMod_Alt = 1 << 14,
	// ImGuiMod_Super = 1 << 15,
	ImGuiMod_CtrlShift = (1 << 12) | (1 << 13),				 // ImGuiMod_Ctrl | ImGuiMod_Shift
	ImGuiMod_CtrlShiftAlt = (1 << 12) | (1 << 13) | (1 << 14), // ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiMod_Alt
	ImGuiMod_ShiftAlt = (1 << 13) | (1 << 14),				 //						 ImGuiMod_Shift | ImGuiMod_Alt
};

//---- Define assertion handler. Defaults to calling assert().
// If your macro uses multiple statements, make sure is enclosed in a 'do { .. } while (0)' block so it can be used as a single statement.
//#define IM_ASSERT(_EXPR)  MyAssert(_EXPR)
//#define IM_ASSERT(_EXPR)  ((void)(_EXPR))     // Disable asserts

//---- Define attributes of all API symbols declarations, e.g. for DLL under Windows
// Using Dear ImGui via a shared library is not recommended, because of function call overhead and because we don't guarantee backward nor forward ABI compatibility.
// - Windows DLL users: heaps and globals are not shared across DLL boundaries! You will need to call SetCurrentContext() + SetAllocatorFunctions()
//   for each static/DLL boundary you are calling from. Read "Context and Memory Allocators" section of imgui.cpp for more details.
//#define IMGUI_API __declspec(dllexport)                   // MSVC Windows: DLL export
//#define IMGUI_API __declspec(dllimport)                   // MSVC Windows: DLL import
//#define IMGUI_API __attribute__((visibility("default")))  // GCC/Clang: override visibility when set is hidden

//---- Don't define obsolete functions/enums/behaviors. Consider enabling from time to time after updating to clean your code of obsolete function/names.
// HACK: Commented out for now specifically to address "SetScreenPos() extending parent boundaries" https://github.com/ocornut/imgui/issues/5548
// #define IMGUI_DISABLE_OBSOLETE_FUNCTIONS

//---- Disable all of Dear ImGui or don't implement standard windows/tools.
// It is very strongly recommended to NOT disable the demo windows and debug tool during development. They are extremely useful in day to day work. Please read comments in imgui_demo.cpp.
//#define IMGUI_DISABLE                                     // Disable everything: all headers and source files will be empty.
//#define IMGUI_DISABLE_DEMO_WINDOWS                        // Disable demo windows: ShowDemoWindow()/ShowStyleEditor() will be empty.
//#define IMGUI_DISABLE_DEBUG_TOOLS                         // Disable metrics/debugger and other debug tools: ShowMetricsWindow(), ShowDebugLogWindow() and ShowIDStackToolWindow() will be empty.

//---- Don't implement some functions to reduce linkage requirements.
//#define IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS   // [Win32] Don't implement default clipboard handler. Won't use and link with OpenClipboard/GetClipboardData/CloseClipboard etc. (user32.lib/.a, kernel32.lib/.a)
//#define IMGUI_ENABLE_WIN32_DEFAULT_IME_FUNCTIONS          // [Win32] [Default with Visual Studio] Implement default IME handler (require imm32.lib/.a, auto-link for Visual Studio, -limm32 on command-line for MinGW)
//#define IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS         // [Win32] [Default with non-Visual Studio compilers] Don't implement default IME handler (won't require imm32.lib/.a)
//#define IMGUI_DISABLE_WIN32_FUNCTIONS                     // [Win32] Won't use and link with any Win32 function (clipboard, IME).
//#define IMGUI_ENABLE_OSX_DEFAULT_CLIPBOARD_FUNCTIONS      // [OSX] Implement default OSX clipboard handler (need to link with '-framework ApplicationServices', this is why this is not the default).
//#define IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS             // Don't implement default platform_io.Platform_OpenInShellFn() handler (Win32: ShellExecute(), require shell32.lib/.a, Mac/Linux: use system("")).
//#define IMGUI_DISABLE_DEFAULT_FORMAT_FUNCTIONS            // Don't implement ImFormatString/ImFormatStringV so you can implement them yourself (e.g. if you don't want to link with vsnprintf)
//#define IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS              // Don't implement ImFabs/ImSqrt/ImPow/ImFmod/ImCos/ImSin/ImAcos/ImAtan2 so you can implement them yourself.
//#define IMGUI_DISABLE_FILE_FUNCTIONS                      // Don't implement ImFileOpen/ImFileClose/ImFileRead/ImFileWrite and ImFileHandle at all (replace them with dummies)
//#define IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS              // Don't implement ImFileOpen/ImFileClose/ImFileRead/ImFileWrite and ImFileHandle so you can implement them yourself if you don't want to link with fopen/fclose/fread/fwrite. This will also disable the LogToTTY() function.
//#define IMGUI_DISABLE_DEFAULT_ALLOCATORS                  // Don't implement default allocators calling malloc()/free() to avoid linking with them. You will need to call ImGui::SetAllocatorFunctions().
//#define IMGUI_DISABLE_DEFAULT_FONT                        // Disable default embedded font (ProggyClean.ttf), remove ~9.5 KB from output binary. AddFontDefault() will assert.
//#define IMGUI_DISABLE_SSE                                 // Disable use of SSE intrinsics even if available

//---- Enable Test Engine / Automation features.
//#define IMGUI_ENABLE_TEST_ENGINE                          // Enable imgui_test_engine hooks. Generally set automatically by include "imgui_te_config.h", see Test Engine for details.

//---- Include imgui_user.h at the end of imgui.h as a convenience
// May be convenient for some users to only explicitly include vanilla imgui.h and have extra stuff included.
//#define IMGUI_INCLUDE_IMGUI_USER_H
//#define IMGUI_USER_H_FILENAME         "my_folder/my_imgui_user.h"

//---- Pack vertex colors as BGRA8 instead of RGBA8 (to avoid converting from one to another). Need dedicated backend support.
//#define IMGUI_USE_BGRA_PACKED_COLOR

//---- Use legacy CRC32-adler tables (used before 1.91.6), in order to preserve old .ini data that you cannot afford to invalidate.
//#define IMGUI_USE_LEGACY_CRC32_ADLER

//---- Use 32-bit for ImWchar (default is 16-bit) to support Unicode planes 1-16. (e.g. point beyond 0xFFFF like emoticons, dingbats, symbols, shapes, ancient languages, etc...)
//#define IMGUI_USE_WCHAR32

//---- Avoid multiple STB libraries implementations, or redefine path/filenames to prioritize another version
// By default the embedded implementations are declared static and not available outside of Dear ImGui sources files.
//#define IMGUI_STB_TRUETYPE_FILENAME   "my_folder/stb_truetype.h"
//#define IMGUI_STB_RECT_PACK_FILENAME  "my_folder/stb_rect_pack.h"
//#define IMGUI_STB_SPRINTF_FILENAME    "my_folder/stb_sprintf.h"    // only used if IMGUI_USE_STB_SPRINTF is defined.
//#define IMGUI_DISABLE_STB_TRUETYPE_IMPLEMENTATION
//#define IMGUI_DISABLE_STB_RECT_PACK_IMPLEMENTATION
//#define IMGUI_DISABLE_STB_SPRINTF_IMPLEMENTATION                   // only disabled if IMGUI_USE_STB_SPRINTF is defined.

//---- Use stb_sprintf.h for a faster implementation of vsnprintf instead of the one from libc (unless IMGUI_DISABLE_DEFAULT_FORMAT_FUNCTIONS is defined)
// Compatibility checks of arguments and formats done by clang and GCC will be disabled in order to support the extra formats provided by stb_sprintf.h.
//#define IMGUI_USE_STB_SPRINTF

//---- Use FreeType to build and rasterize the font atlas (instead of stb_truetype which is embedded by default in Dear ImGui)
// Requires FreeType headers to be available in the include path. Requires program to be compiled with 'misc/freetype/imgui_freetype.cpp' (in this repository) + the FreeType library (not provided).
// On Windows you may use vcpkg with 'vcpkg install freetype --triplet=x64-windows' + 'vcpkg integrate install'.
//#define IMGUI_ENABLE_FREETYPE

//---- Use FreeType + plutosvg or lunasvg to render OpenType SVG fonts (SVGinOT)
// Only works in combination with IMGUI_ENABLE_FREETYPE.
// - plutosvg is currently easier to install, as e.g. it is part of vcpkg. It will support more fonts and may load them faster. See misc/freetype/README for instructions.
// - Both require headers to be available in the include path + program to be linked with the library code (not provided).
// - (note: lunasvg implementation is based on Freetype's rsvg-port.c which is licensed under CeCILL-C Free Software License Agreement)
//#define IMGUI_ENABLE_FREETYPE_PLUTOSVG
//#define IMGUI_ENABLE_FREETYPE_LUNASVG

//---- Use stb_truetype to build and rasterize the font atlas (default)
// The only purpose of this define is if you want force compilation of the stb_truetype backend ALONG with the FreeType backend.
//#define IMGUI_ENABLE_STB_TRUETYPE

//---- Define constructor and implicit cast operators to convert back<>forth between your math types and ImVec2/ImVec4.
// This will be inlined as part of ImVec2 and ImVec4 class declarations.
#define IM_VEC2_CLASS_EXTRA                                                     \
        constexpr ImVec2(const vec2& f) : x(f.x), y(f.y) {}						\
        operator vec2() const { return vec2(x, y); }

/*
#define IM_VEC4_CLASS_EXTRA                                                     \
		constexpr ImVec4(const MyVec4& f) : x(f.x), y(f.y), z(f.z), w(f.w) {}   \
		operator MyVec4() const { return MyVec4(x,y,z,w); }
*/
//---- ...Or use Dear ImGui's own very basic math operators.
//#define IMGUI_DEFINE_MATH_OPERATORS

//---- Use 32-bit vertex indices (default is 16-bit) is one way to allow large meshes with more than 64K vertices.
// Your renderer backend will need to support it (most example renderer backends support both 16/32-bit indices).
// Another way to allow large meshes while keeping 16-bit indices is to handle ImDrawCmd::VtxOffset in your renderer.
// Read about ImGuiBackendFlags_RendererHasVtxOffset for details.
//#define ImDrawIdx unsigned int

//---- Override ImDrawCallback signature (will need to modify renderer backends accordingly)
//struct ImDrawList;
//struct ImDrawCmd;
//typedef void (*MyImDrawCallback)(const ImDrawList* draw_list, const ImDrawCmd* cmd, void* my_renderer_user_data);
//#define ImDrawCallback MyImDrawCallback

//---- Debug Tools: Macro to break in Debugger (we provide a default implementation of this in the codebase)
// (use 'Metrics->Tools->Item Picker' to pick widgets with the mouse and break into them for easy debugging.)
//#define IM_DEBUG_BREAK  IM_ASSERT(0)
//#define IM_DEBUG_BREAK  __debugbreak()

//---- Debug Tools: Enable slower asserts
//#define IMGUI_DEBUG_PARANOID

//---- Tip: You can add extra functions within the ImGui:: namespace from anywhere (e.g. your own sources/header files)
/*
namespace ImGui
{
	void MyFunction(const char* name, MyMatrix44* mtx);
}
*/
