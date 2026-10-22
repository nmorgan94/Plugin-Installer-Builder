#include "TitleBar.h"

#import <AppKit/AppKit.h>

void useDarkTitleBar (juce::ComponentPeer& peer)
{
    auto* view = static_cast<NSView*> (peer.getNativeHandle());
    view.window.appearance = [NSAppearance appearanceNamed: NSAppearanceNameDarkAqua];
}
