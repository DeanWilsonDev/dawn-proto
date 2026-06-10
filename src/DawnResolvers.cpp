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

Penumbra::Widgets::ButtonStyle ResolvePaletteButtonStyle(const Theme& Theme) {
    Penumbra::Widgets::ButtonStyle Style;
    Style.ColorBackground         = Theme.ColorAccent;
    Style.ColorBackgroundHovered  = Theme.ColorAccentHovered;
    Style.ColorBackgroundPressed  = Theme.ColorAccentPressed;
    Style.ColorBackgroundDisabled = Theme.ColorSurfaceRaised;
    Style.ColorLabel              = Theme.ColorBackgroundBase; // dark text on amber
    Style.ColorBorder             = Theme.ColorBorderSubtle;
    Style.BorderWidth             = Theme.BorderWidthDefault;
    Style.BorderRadius            = Theme.BorderRadiusButton;
    Style.Padding                 = {Theme.SpacingMedium, Theme.SpacingSmall,
                                     Theme.SpacingMedium, Theme.SpacingSmall};
    return Style;
}

Penumbra::Widgets::ButtonStyle ResolveListButtonStyle(const Theme& Theme) {
    Penumbra::Widgets::ButtonStyle Style;
    Style.ColorBackground         = {0, 0, 0, 0}; // transparent: reads as a flat row
    Style.ColorBackgroundHovered  = Theme.ColorSurfaceRaised;
    Style.ColorBackgroundPressed  = Theme.ColorSurfaceOverlay;
    Style.ColorBackgroundDisabled = {0, 0, 0, 0};
    Style.ColorLabel              = Theme.ColorTextPrimary;
    Style.BorderWidth             = 0.0f;
    Style.BorderRadius            = Theme.BorderRadiusButton;
    Style.Padding                 = {Theme.SpacingSmall, Theme.SpacingSmall,
                                     Theme.SpacingSmall, Theme.SpacingSmall};
    return Style;
}

Penumbra::Widgets::BoxStyle ResolveInputFieldStyle(const Theme& Theme) {
    Penumbra::Widgets::BoxStyle Style;
    Style.ColorBackground = Theme.ColorBackgroundBase;
    Style.ColorBorder     = Theme.ColorBorderSubtle;
    Style.BorderWidth     = Theme.BorderWidthDefault;
    Style.BorderRadius    = Theme.BorderRadiusButton;
    Style.Padding         = {Theme.SpacingSmall, Theme.SpacingSmall,
                             Theme.SpacingSmall, Theme.SpacingSmall};
    return Style;
}

} // namespace Dawn
