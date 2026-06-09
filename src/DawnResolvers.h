#pragma once

#include "DawnTheme.h"

#include "Penumbra/Widgets/Styles.h"

namespace Dawn {

// Resolvers map Dawn's semantic intent onto concrete Penumbra style structs. This
// is the seam the brief calls out: Dawn owns the look, Penumbra only interprets it.
// New resolvers are added as later milestones need them (buttons, input fields, ...).

// A side panel: raised surface, subtle border, medium padding.
Penumbra::Widgets::BoxStyle ResolvePanelStyle(const Theme&);

// The top header bar carrying the project title and (later) the dirty indicator.
Penumbra::Widgets::BoxStyle ResolveHeaderBarStyle(const Theme&);

// The central viewport frame. SceneClearColor is set separately on the widget;
// this only styles the frame (background shown behind/around the scene texture).
Penumbra::Widgets::BoxStyle ResolveViewportStyle(const Theme&);

} // namespace Dawn
