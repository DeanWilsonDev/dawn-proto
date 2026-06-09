#pragma once

#include "DawnTheme.h"

#include <string>

namespace Dawn {

// Owns everything on the Penumbra side of Dawn: the platform window, the renderer,
// the font backend, the widget tree, and the frame loop. This is the only Dawn
// translation unit that touches Penumbra's window/render layers, keeping the "Dawn
// never calls SDL directly" boundary in one place. main() stays thin: it sets up
// logging, reads the .dawn project file, then hands control here.
//
// M0 builds only the layout shell (header bar + left panel + viewport placeholder +
// right panel). Later milestones grow this class with the camera, selection, command
// stack, and the scene document.
class EditorApplication {
public:
    EditorApplication(Theme Theme, std::string ProjectName);

    // Opens the window and runs the frame loop until the user quits.
    // Returns the process exit code (0 on a clean shutdown, non-zero on init failure).
    int Run();

private:
    Theme       AppTheme;
    std::string ProjectName;
};

} // namespace Dawn
