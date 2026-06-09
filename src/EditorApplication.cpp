#include "DawnLog.h"

#include "EditorApplication.h"
#include "DawnResolvers.h"

#include "Penumbra/Platform/PlatformWindow.h"
#include "Penumbra/Render/Renderer.h"
#include "Penumbra/Render/SdlTtfFontBackend.h"
#include "Penumbra/Widgets/Box.h"
#include "Penumbra/Widgets/Label.h"
#include "Penumbra/Widgets/ViewportWidget.h"

#include <memory>
#include <string>
#include <utility>

namespace Dawn {

namespace {

// Initial logical window size. A Dawn concern, never a Penumbra concern.
constexpr int WindowLogicalWidth  = 1280;
constexpr int WindowLogicalHeight = 800;

constexpr const char* FontFileName = "JetBrainsMonoNerdFontMono-Regular.ttf";

} // namespace

EditorApplication::EditorApplication(Theme Theme, std::string ProjectName)
    : AppTheme(Theme), ProjectName(std::move(ProjectName)) {}

int EditorApplication::Run() {
    using namespace Penumbra::Widgets;

    Penumbra::Platform::PlatformWindow Window;
    if (!Window.Initialise("Dawn", WindowLogicalWidth, WindowLogicalHeight)) {
        LOG_ERROR("Failed to initialise platform window: {}", SDL_GetError());
        return 1;
    }
    LOG_INFO("Editor window opened ({}x{} logical, DPI scale {})", WindowLogicalWidth,
             WindowLogicalHeight, Window.GetDpiScaleFactor());

    {
        // The font backend and renderer must be torn down while the SDL_Renderer is
        // still alive, so they live in a scope that closes before Window.Shutdown().
        Penumbra::Render::SdlTtfFontBackend FontBackend;
        const std::string FontPath = std::string(DAWN_ASSET_DIR) + "/" + FontFileName;
        const Penumbra::Render::FontHandle BodyFont =
            FontBackend.LoadFont(FontPath.c_str(), AppTheme.FontSizeBody, Window.GetDpiScaleFactor());

        Penumbra::Render::Renderer Renderer;
        if (!Renderer.Initialise(Window.GetSdlRenderer(), Window.GetDpiScaleFactor(), &FontBackend)) {
            LOG_ERROR("Failed to initialise renderer");
            Window.Shutdown();
            return 1;
        }

        // ---- composition helpers (Dawn's stand-in for UmbraComponentLibrary) ----
        auto MakeLabel = [&](const std::string& Text, SDL_Color Color) {
            auto Widget = std::make_unique<Label>();
            Widget->FontBackend = &FontBackend;
            Widget->Font        = BodyFont;
            Widget->Text        = Text;
            Widget->ColorText   = Color;
            return Widget;
        };

        // ---- header bar: project title (dirty indicator arrives in M4) ----
        auto HeaderBar = std::make_unique<Box>();
        HeaderBar->Style          = ResolveHeaderBarStyle(AppTheme);
        HeaderBar->Layout         = LayoutMode::HorizontalStack;
        HeaderBar->CrossAlignment = CrossAlign::Center;
        HeaderBar->ChildGap       = AppTheme.SpacingSmall;
        HeaderBar->AddChild(MakeLabel(ProjectName, AppTheme.ColorTextPrimary));

        // ---- left panel: entity palette / list (placeholder in M0) ----
        auto LeftPanel = std::make_unique<Box>();
        LeftPanel->Style          = ResolvePanelStyle(AppTheme);
        LeftPanel->Layout         = LayoutMode::VerticalStack;
        LeftPanel->CrossAlignment = CrossAlign::Stretch;
        LeftPanel->ChildGap       = AppTheme.SpacingSmall;
        LeftPanel->AddChild(MakeLabel("Entities", AppTheme.ColorTextSecondary));

        // ---- central viewport placeholder: a dark scene area, no scene yet ----
        auto Viewport = std::make_unique<ViewportWidget>();
        Viewport->Style           = ResolveViewportStyle(AppTheme);
        Viewport->SceneClearColor = AppTheme.ColorBackgroundBase;
        // OnRenderScene is intentionally unset in M0 — the viewport clears to the
        // scene background and shows an empty stage. Scene drawing arrives in M2.

        // ---- right panel: properties (placeholder in M0) ----
        auto RightPanel = std::make_unique<Box>();
        RightPanel->Style          = ResolvePanelStyle(AppTheme);
        RightPanel->Layout         = LayoutMode::VerticalStack;
        RightPanel->CrossAlignment = CrossAlign::Stretch;
        RightPanel->ChildGap       = AppTheme.SpacingSmall;
        RightPanel->AddChild(MakeLabel("Properties", AppTheme.ColorTextSecondary));

        Penumbra::Platform::InputState Input;
        bool KeepRunning = true;
        while (KeepRunning) {
            KeepRunning = Window.PumpEventsAndBuildInput(Input);

            // Manual three-column layout. Box stacking has no flex/grow, so (like the
            // Penumbra demo) Dawn computes each top-level widget's rect and arranges
            // it explicitly. This re-runs every frame, so window resize just works.
            const SDL_FPoint WindowSize = Window.GetLogicalWindowSize();
            const float M = AppTheme.SpacingMedium; // outer margin
            const float G = AppTheme.SpacingMedium; // gap between regions
            const float FullW = WindowSize.x - 2.0f * M;

            const float HeaderH = HeaderBar->Measure({FullW, WindowSize.y}).y;
            const SDL_FRect HeaderRect{M, M, FullW, HeaderH};

            const float BodyTop = M + HeaderH + G;
            const float BodyH   = WindowSize.y - BodyTop - M;
            const float SideW   = AppTheme.PanelContentWidth;
            const float CenterX = M + SideW + G;
            const float CenterW = FullW - 2.0f * SideW - 2.0f * G;
            const float RightX  = M + FullW - SideW;

            const SDL_FRect LeftRect{M, BodyTop, SideW, BodyH};
            const SDL_FRect ViewportRect{CenterX, BodyTop, CenterW, BodyH};
            const SDL_FRect RightRect{RightX, BodyTop, SideW, BodyH};

            HeaderBar->Measure({HeaderRect.w, HeaderRect.h});
            HeaderBar->Arrange(HeaderRect);
            LeftPanel->Measure({LeftRect.w, LeftRect.h});
            LeftPanel->Arrange(LeftRect);
            Viewport->Measure({ViewportRect.w, ViewportRect.h});
            Viewport->Arrange(ViewportRect);
            RightPanel->Measure({RightRect.w, RightRect.h});
            RightPanel->Arrange(RightRect);

            // Route input. Topmost / most-specific regions get first refusal.
            if (!HeaderBar->UpdateInteractionState(Input) &&
                !LeftPanel->UpdateInteractionState(Input) &&
                !RightPanel->UpdateInteractionState(Input)) {
                Viewport->UpdateInteractionState(Input);
            }

            Renderer.BeginFrame(AppTheme.ColorBackgroundBase);
            HeaderBar->Draw(Renderer);
            LeftPanel->Draw(Renderer);
            Viewport->Draw(Renderer);
            RightPanel->Draw(Renderer);
            Renderer.EndFrameAndPresent();
        }
    }

    LOG_INFO("Editor shutting down");
    Window.Shutdown();
    return 0;
}

} // namespace Dawn
