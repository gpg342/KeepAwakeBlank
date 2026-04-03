KeepAwake Plus - Native C++ Win32 App
=====================================

What it does
------------
- Prevents screen saver, display sleep, and system sleep while Keep Awake is ON.
- Toggles a full-screen overlay on the second monitor with Ctrl+Alt+W.
- Overlay colour can be White, Black, or a Custom colour chosen from a picker.
- Lives in the system tray.
- Closing the window hides it to the tray. Use the tray menu to exit.

Hotkeys
-------
- Ctrl+Alt+K  -> Toggle Keep Awake ON / OFF
- Ctrl+Alt+W  -> Toggle second monitor overlay ON / OFF

Tray menu
---------
- Keep Awake ON / OFF
- Overlay ON / OFF
- Overlay Colour -> White / Black / Choose Custom...
- Show Window
- Exit

Important notes
---------------
- The overlay works when Windows is set to Extend displays and a second monitor is detected.
- Ctrl+W was avoided because many programs use it to close tabs or windows.
- The overlay is a topmost full-screen window on monitor 2. It does not change Windows display settings.

Build in VS Code using MSVC
---------------------------
1. Install the Desktop development with C++ workload.
2. Open the "x64 Native Tools Command Prompt for VS 2022".
3. Change into this folder.
4. Run:

   cl /O2 /EHsc /DUNICODE /D_UNICODE KeepAwakePlus.cpp user32.lib shell32.lib gdi32.lib comdlg32.lib

5. This creates:

   KeepAwakePlus.exe

Build in full Visual Studio
---------------------------
- Open the folder or create an empty Win32 Desktop project.
- Add KeepAwakePlus.cpp.
- Build in Release | x64.
