#include "DawnLog.h"

#include "EditorApplication.h"
#include "DawnResolvers.h"

#include "Penumbra/Platform/PlatformWindow.h"
#include "Penumbra/Render/Renderer.h"
#include "Penumbra/Render/SdlTtfFontBackend.h"
#include "Penumbra/Widgets/Box.h"
#include "Penumbra/Widgets/Label.h"
#include "Penumbra/Widgets/ScrollablePanel.h"
#include "Penumbra/Widgets/ViewportWidget.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace Dawn {

namespace {

// Initial logical window size. A Dawn concern, never a Penumbra concern.
constexpr int WindowLogicalWidth  = 1280;
constexpr int WindowLogicalHeight = 800;

constexpr const char* FontFileName = "JetBrainsMonoNerdFontMono-Regular.ttf";

// The viewport camera. Dawn owns this; the ViewportWidget provides only the render
// seam. World→screen is (world - Offset) * Zoom, where screen is relative to the
// scene texture's top-left (the content rect origin).
struct CameraState {
    float OffsetX{0.0f};
    float OffsetY{0.0f};
    float Zoom{1.0f};
};

SDL_Color ToSdl(Color C) { return {C.R, C.G, C.B, C.A}; }

// Frames all entities: centres their bounding box in the content area at a zoom that
// fits with margin. Run once when the scene opens so the rectangles are visible
// without the user hunting for them. Clamped to the same zoom range as wheel zoom.
void FrameEntities(CameraState& Camera, const SceneDocument& Scene, SDL_FPoint Content) {
    if (Scene.Entities.empty() || Content.x <= 1.0f || Content.y <= 1.0f) {
        return;
    }
    const auto& First = Scene.Entities.front();
    float MinX = static_cast<float>(First.PositionX);
    float MinY = static_cast<float>(First.PositionY);
    float MaxX = static_cast<float>(First.PositionX + First.Width);
    float MaxY = static_cast<float>(First.PositionY + First.Height);
    for (const auto& E : Scene.Entities) {
        MinX = std::min(MinX, static_cast<float>(E.PositionX));
        MinY = std::min(MinY, static_cast<float>(E.PositionY));
        MaxX = std::max(MaxX, static_cast<float>(E.PositionX + E.Width));
        MaxY = std::max(MaxY, static_cast<float>(E.PositionY + E.Height));
    }
    const float BoxW = std::max(MaxX - MinX, 1.0f);
    const float BoxH = std::max(MaxY - MinY, 1.0f);
    constexpr float Margin = 0.8f; // leave a border around the framed content
    const float Zoom =
        std::clamp(std::min(Content.x * Margin / BoxW, Content.y * Margin / BoxH), 0.1f, 10.0f);
    const float CentreX = (MinX + MaxX) * 0.5f;
    const float CentreY = (MinY + MaxY) * 0.5f;
    Camera.Zoom    = Zoom;
    Camera.OffsetX = CentreX - (Content.x * 0.5f) / Zoom;
    Camera.OffsetY = CentreY - (Content.y * 0.5f) / Zoom;
}

} // namespace

EditorApplication::EditorApplication(Theme Theme, ProjectData Project, EntitySchema Schema,
                                     SceneDocument Scene)
    : AppTheme(Theme), Project(std::move(Project)), Schema(std::move(Schema)),
      Scene(std::move(Scene)) {}

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
        HeaderBar->AddChild(MakeLabel(Project.Name, AppTheme.ColorTextPrimary));

        // Resolves an entity's draw colour from its type via the schema. Colour is a
        // property of the type, not stored on the entity (keeps the model SDL-free).
        auto ColorForType = [&](const std::string& Type) -> SDL_Color {
            if (const EntityTypeInfo* Info = Schema.Find(Type)) {
                return ToSdl(Info->EditorColor);
            }
            return AppTheme.ColorTextSecondary; // unknown type → neutral grey
        };

        // ---- left panel: scrollable entity list (one Label per entity) ----
        auto LeftPanel = std::make_unique<ScrollablePanel>();
        LeftPanel->Style            = ResolvePanelStyle(AppTheme);
        LeftPanel->CrossAlignment   = CrossAlign::Stretch;
        LeftPanel->ChildGap         = AppTheme.SpacingSmall;
        LeftPanel->WheelStepLogical = AppTheme.SpacingLarge * 3.0f;
        LeftPanel->AddChild(MakeLabel("Entities", AppTheme.ColorTextSecondary));
        for (const auto& Entity : Scene.Entities) {
            LeftPanel->AddChild(MakeLabel(Entity.Name, AppTheme.ColorTextPrimary));
        }

        // ---- central viewport: entities as coloured rectangles, with a camera ----
        CameraState Camera;
        SDL_FPoint  LastMousePos{0.0f, 0.0f}; // for middle-drag pan deltas

        auto Viewport = std::make_unique<ViewportWidget>();
        Viewport->Style           = ResolveViewportStyle(AppTheme);
        Viewport->SceneClearColor = AppTheme.ColorBackgroundBase;

        // Draw each entity as a coloured rectangle at its camera-transformed position.
        // The Renderer is already targeting the scene texture; coordinates are logical
        // pixels relative to the content rect's top-left.
        Viewport->OnRenderScene = [&](Penumbra::Render::Renderer& SceneRenderer, SDL_FPoint) {
            for (const auto& Entity : Scene.Entities) {
                const float ScreenX = (static_cast<float>(Entity.PositionX) - Camera.OffsetX) * Camera.Zoom;
                const float ScreenY = (static_cast<float>(Entity.PositionY) - Camera.OffsetY) * Camera.Zoom;
                const float ScreenW = static_cast<float>(Entity.Width) * Camera.Zoom;
                const float ScreenH = static_cast<float>(Entity.Height) * Camera.Zoom;
                SceneRenderer.DrawFilledRect({ScreenX, ScreenY, ScreenW, ScreenH},
                                             ColorForType(Entity.Type));
            }
        };

        // Pan with middle-mouse drag; zoom with the wheel, keeping the point under the
        // cursor fixed in world space.
        Viewport->OnSceneInput = [&](const Penumbra::Platform::InputState& Input,
                                     SDL_FRect ContentRect) -> bool {
            if (Input.MouseButtonDown[1]) { // middle button
                Camera.OffsetX -= (Input.MousePosition.x - LastMousePos.x) / Camera.Zoom;
                Camera.OffsetY -= (Input.MousePosition.y - LastMousePos.y) / Camera.Zoom;
            }
            if (Input.MouseWheelDelta != 0.0f) {
                const float WorldX = (Input.MousePosition.x - ContentRect.x) / Camera.Zoom + Camera.OffsetX;
                const float WorldY = (Input.MousePosition.y - ContentRect.y) / Camera.Zoom + Camera.OffsetY;
                Camera.Zoom = std::clamp(Camera.Zoom * (1.0f + Input.MouseWheelDelta * 0.1f), 0.1f, 10.0f);
                Camera.OffsetX = WorldX - (Input.MousePosition.x - ContentRect.x) / Camera.Zoom;
                Camera.OffsetY = WorldY - (Input.MousePosition.y - ContentRect.y) / Camera.Zoom;
            }
            LastMousePos = Input.MousePosition;
            return true;
        };

        // ---- right panel: properties (placeholder in M0) ----
        auto RightPanel = std::make_unique<Box>();
        RightPanel->Style          = ResolvePanelStyle(AppTheme);
        RightPanel->Layout         = LayoutMode::VerticalStack;
        RightPanel->CrossAlignment = CrossAlign::Stretch;
        RightPanel->ChildGap       = AppTheme.SpacingSmall;
        RightPanel->AddChild(MakeLabel("Properties", AppTheme.ColorTextSecondary));

        Penumbra::Platform::InputState Input;
        bool       SceneFramed = false;
        SDL_FPoint LastViewportSize{-1.0f, -1.0f}; // forces a resize log on frame 1
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

            // The scene content area (inside the viewport's border) — the space
            // OnRenderScene draws into and the camera maps world coordinates onto.
            const float Border = AppTheme.BorderWidthDefault;
            const SDL_FPoint ViewportContent{ViewportRect.w - 2.0f * Border,
                                             ViewportRect.h - 2.0f * Border};

            if (ViewportRect.w != LastViewportSize.x || ViewportRect.h != LastViewportSize.y) {
                LOG_TRACE("Viewport resized to {}x{}", ViewportRect.w, ViewportRect.h);
                LastViewportSize = {ViewportRect.w, ViewportRect.h};
            }

            // Frame the scene once, when the content area is first known.
            if (!SceneFramed && ViewportContent.x > 1.0f && ViewportContent.y > 1.0f) {
                FrameEntities(Camera, Scene, ViewportContent);
                SceneFramed = true;
                LOG_DEBUG("Framed {} entit{} at zoom {}", Scene.Entities.size(),
                          Scene.Entities.size() == 1 ? "y" : "ies", Camera.Zoom);
            }

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
