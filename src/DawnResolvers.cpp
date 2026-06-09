#include "DawnResolvers.h"

namespace Dawn {

Penumbra::Widgets::BoxStyle ResolvePanelStyle(const Theme& Theme) {
    Penumbra::Widgets::BoxStyle Style;
    Style.ColorBackground = Theme.ColorSurface;
    Style.ColorBorder     = Theme.ColorBorderSubtle;
    Style.BorderWidth     = Theme.BorderWidthDefault;
    Style.BorderRadius    = Theme.BorderRadiusPanel;
    Style.Padding         = {Theme.SpacingMedium, Theme.SpacingMedium,
                             Theme.SpacingMedium, Theme.SpacingMedium};
    return Style;
}

Penumbra::Widgets::BoxStyle ResolveHeaderBarStyle(const Theme& Theme) {
    Penumbra::Widgets::BoxStyle Style;
    Style.ColorBackground = Theme.ColorSurfaceRaised;
    Style.ColorBorder     = Theme.ColorBorderSubtle;
    Style.BorderWidth     = Theme.BorderWidthDefault;
    Style.BorderRadius    = Theme.BorderRadiusPanel;
    Style.Padding         = {Theme.SpacingMedium, Theme.SpacingSmall,
                             Theme.SpacingMedium, Theme.SpacingSmall};
    return Style;
}

Penumbra::Widgets::BoxStyle ResolveViewportStyle(const Theme& Theme) {
    Penumbra::Widgets::BoxStyle Style;
    Style.ColorBackground = Theme.ColorBackgroundBase;
    Style.ColorBorder     = Theme.ColorBorderSubtle;
    Style.BorderWidth     = Theme.BorderWidthDefault;
    Style.BorderRadius    = Theme.BorderRadiusPanel;
    return Style;
}

} // namespace Dawn
