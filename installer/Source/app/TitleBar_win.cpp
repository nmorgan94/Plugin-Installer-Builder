#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>

#include "TitleBar.h"

#pragma comment (lib, "dwmapi.lib")

void useDarkTitleBar (juce::ComponentPeer& peer)
{
    // Windows 10 20H1 and later; older versions ignore it
    const BOOL dark = TRUE;
    DwmSetWindowAttribute (static_cast<HWND> (peer.getNativeHandle()), DWMWA_USE_IMMERSIVE_DARK_MODE,
                           &dark, sizeof (dark));
}
