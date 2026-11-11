# Insta360 X5 Control Center - Windows starter

This is a Qt 6 / C++ Windows GUI wired to the **user-supplied Insta360 Camera SDK 2.1.8**. It is source code, not a prebuilt EXE. It has not been compiled or tested with a physical X5 in this environment.

## Current functions
- Detect the first camera reported by the official SDK and open/close it.
- Take photo, start recording, stop recording using the SDK methods.
- Basic error/status messages.

## Limitations
- No live preview or interactive stitched 360-degree viewer yet. The Camera SDK preview is an encoded stream requiring decoding; interactive stitching may need the Media SDK or a separate rendering pipeline.
- Video mode/capture parameters are not configured by this starter; default camera state determines whether recording works.
- The SDK may require its own USB driver and a specific camera USB mode. Consult the SDK's official documentation.
- SDK DLL/lib/headers in this ZIP are provided by the user and may be subject to Insta360's licensing restrictions; do not redistribute without permission.

## Build on Windows
1. Install Visual Studio 2022 with Desktop development with C++ workload, Qt 6 for MSVC 2022 64-bit, and CMake.
2. Extract ZIP to a folder.
3. Open a Developer PowerShell for VS 2022 in the extracted folder.
4. Run `cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"` (adjust Qt version/path).
5. Run `cmake --build build --config Release`.
6. Deploy Qt runtime DLLs using `windeployqt build/Release/X5ControlCenter.exe` from the Qt MSVC command prompt.
7. Connect X5 with a data-capable USB-C cable, select the USB mode required by the Camera SDK, and run the executable.

## Notes
The included `CameraSDKDemo.exe` from Insta360 is also useful to validate USB camera detection independently before running the GUI.
