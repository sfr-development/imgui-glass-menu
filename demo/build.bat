@echo off
setlocal
set ROOT=%~dp0..
set OUT=%~dp0bin
if not exist "%OUT%" mkdir "%OUT%"

where cl >nul 2>nul || call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul

cl /nologo /std:c++17 /EHsc /O2 /MD /DUNICODE /D_UNICODE ^
   /I"%ROOT%" /I"%ROOT%\src" /I"%ROOT%\imgui" ^
   /Fo"%OUT%\\" /Fe"%OUT%\glass_menu_demo.exe" ^
   "%~dp0demo.cpp" ^
   "%ROOT%\src\ui_layout.cpp" "%ROOT%\src\ui_anim.cpp" "%ROOT%\src\lucide_font.cpp" ^
   "%ROOT%\src\ui_config.cpp" "%ROOT%\src\ui_sound.cpp" "%ROOT%\src\ui_overlay.cpp" "%ROOT%\src\ui_hud.cpp" ^
   "%ROOT%\src\ui_registry.cpp" "%ROOT%\src\ui_toast.cpp" "%ROOT%\src\ui_gfx.cpp" "%ROOT%\src\ui_palette.cpp" ^
   "%ROOT%\imgui\imgui.cpp" "%ROOT%\imgui\imgui_draw.cpp" "%ROOT%\imgui\imgui_widgets.cpp" "%ROOT%\imgui\imgui_tables.cpp" ^
   "%ROOT%\imgui\backends\imgui_impl_win32.cpp" "%ROOT%\imgui\backends\imgui_impl_dx11.cpp" ^
   /link /SUBSYSTEM:WINDOWS d3d11.lib d3dcompiler.lib dxgi.lib user32.lib gdi32.lib dwmapi.lib
endlocal
