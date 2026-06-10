#include "DawnLog.h"

#include "EditorApplication.h"
#include "DawnResolvers.h"
#include "SceneCommand.h"

#include "Penumbra/Platform/PlatformWindow.h"
#include "Penumbra/Render/Renderer.h"
#include "Penumbra/Render/SdlTtfFontBackend.h"
#include "Penumbra/Widgets/Box.h"
#include "Penumbra/Widgets/Button.h"
#include "Penumbra/Widgets/FocusState.h"
#include "Penumbra/Widgets/Label.h"
#include "Penumbra/Widgets/NumericDrag.h"
#include "Penumbra/Widgets/ScrollablePanel.h"
#include "Penumbra/Widgets/TextInput.h"
#include "Penumbra/Widgets/ViewportWidget.h"

#include <amanuensis.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace Dawn {

namespace {

// Initial logical window size. A Dawn concern, never a Penumbra concern.
constexpr int WindowLogicalWidth  = 1280;
constexpr int WindowLogicalHeight = 800;

constexpr const char* FontFileName = "JetBrainsMonoNerdFontMono-Regular.ttf";

// Properties-panel field tuning (kept local; not part of the brief's Theme).
constexpr float FieldWidthLogical = 120.0f;
constexpr float DragSensitivity   = 0.5f; // world px per logical px dragged

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
                                     SceneDocument Scene, std::string ScenePath)
    : AppTheme(Theme), Project(std::move(Project)), Schema(std::move(Schema)),
      Scene(std::move(Scene)), ScenePath(std::move(ScenePath)) {}

int EditorApplication::Run() {
    using namespace Penumbra::Widgets;
    using Penumbra::Platform::InputState;

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

        // ===================================================================
        // Editor session state. Lives in Run() and is captured by reference in
        // the widget callbacks (which outlive no longer than the widgets, which
        // live for the duration of Run()).
        // ===================================================================
        CommandStack Stack;
        CameraState  Camera;
        FocusState   Focus;
        SDL_FPoint   LastMousePos{0.0f, 0.0f};

        std::string SelectedId;        // empty = nothing selected
        std::string ArmedType;         // non-empty = next viewport click places this type
        int         PlacementCounter = 0;

        // Drag-to-move state.
        bool        Dragging = false;
        std::string DraggedId;
        float       GrabOffsetX = 0.0f;
        float       GrabOffsetY = 0.0f;

        // Pending live edit (drag/property), flushed into a command at boundaries so
        // each interaction becomes one undoable step.
        bool        PendingActive = false;
        std::string PendingId;
        std::string PendingName;
        double      PendingX = 0.0;
        double      PendingY = 0.0;

        // Deferred requests: widget callbacks set these; the tree is mutated only
        // after the input pass so we never destroy a widget mid-callback.
        bool        RequestPlace = false;
        std::string RequestPlaceType;
        float       RequestPlaceX = 0.0f;
        float       RequestPlaceY = 0.0f;
        bool        RequestSelect = false;
        std::string RequestSelectId;
        bool        RequestClearSelection = false;
        bool        RequestUndo = false;
        bool        RequestRedo = false;
        bool        RequestRebuildList = false;
        bool        RequestRebuildProps = false;

        // Save / reload (M4).
        bool        RequestSave = false;
        bool        RequestReload = false;          // user asked to reload
        bool        RequestReloadConfirmed = false; // confirmed past the dirty prompt
        bool        ConfirmingReload = false;       // a dirty-discard prompt is showing
        bool        SceneFramed = false;            // false re-frames on the next frame
        std::string StatusMessage;                  // transient header status / error text
        SDL_Color   StatusColor = AppTheme.ColorTextSecondary;

        // ---- composition helpers ----
        auto MakeLabel = [&](const std::string& Text, SDL_Color Color) {
            auto Widget = std::make_unique<Label>();
            Widget->FontBackend = &FontBackend;
            Widget->Font        = BodyFont;
            Widget->Text        = Text;
            Widget->ColorText   = Color;
            return Widget;
        };
        auto MakeRow = [&]() {
            auto Row = std::make_unique<Box>();
            Row->Layout         = LayoutMode::HorizontalStack;
            Row->ChildGap       = AppTheme.SpacingSmall;
            Row->CrossAlignment = CrossAlign::Center;
            return Row;
        };
        auto MakeSeparator = [&]() {
            auto Line = std::make_unique<Box>();
            Line->Style.ColorBackground = AppTheme.ColorBorderSubtle;
            Line->Style.Padding         = {0.0f, AppTheme.BorderWidthDefault, 0.0f, 0.0f};
            return Line;
        };
        auto MakeButton = [&](const ButtonStyle& Style, const std::string& Text,
                              SDL_Color LabelColor,
                              std::function<void()> OnClick) -> std::unique_ptr<Button> {
            auto Btn = std::make_unique<Button>();
            Btn->ApplyStyle(Style);
            Btn->Layout                      = LayoutMode::HorizontalStack;
            Btn->CrossAlignment              = CrossAlign::Center;
            Btn->BackgroundTransitionSeconds = AppTheme.AnimColorSeconds;
            Btn->OnClicked                   = std::move(OnClick);
            Btn->AddChild(MakeLabel(Text, LabelColor));
            return Btn;
        };

        auto ColorForType = [&](const std::string& Type) -> SDL_Color {
            if (const EntityTypeInfo* Info = Schema.Find(Type)) {
                return ToSdl(Info->EditorColor);
            }
            return AppTheme.ColorTextSecondary; // unknown type → neutral grey
        };

        // ---- header bar: project title, dirty indicator, status / prompt line ----
        auto HeaderBar = std::make_unique<Box>();
        HeaderBar->Style          = ResolveHeaderBarStyle(AppTheme);
        HeaderBar->Layout         = LayoutMode::HorizontalStack;
        HeaderBar->CrossAlignment = CrossAlign::Center;
        HeaderBar->ChildGap       = AppTheme.SpacingSmall;
        HeaderBar->AddChild(MakeLabel(Project.Name, AppTheme.ColorTextPrimary));
        Label* DirtyLabel = static_cast<Label*>(
            HeaderBar->AddChild(MakeLabel("", AppTheme.ColorWarning)));   // "*" when dirty
        Label* StatusLabel = static_cast<Label*>(
            HeaderBar->AddChild(MakeLabel("", AppTheme.ColorTextSecondary))); // messages / prompt

        // ---- left column: entity palette + scrollable entity list ----
        auto LeftPanel            = std::make_unique<ScrollablePanel>();
        LeftPanel->Style            = ResolvePanelStyle(AppTheme);
        LeftPanel->CrossAlignment   = CrossAlign::Stretch;
        LeftPanel->ChildGap         = AppTheme.SpacingSmall;
        LeftPanel->WheelStepLogical = AppTheme.SpacingLarge * 3.0f;
        ScrollablePanel* LeftPanelPtr = LeftPanel.get();

        auto RebuildLeftPanel = [&]() {
            LeftPanelPtr->Children.clear();
            LeftPanelPtr->AddChild(MakeLabel("Palette", AppTheme.ColorTextSecondary));
            for (const auto& Type : Schema.Types) {
                const std::string Id = Type.Id;
                LeftPanelPtr->AddChild(MakeButton(
                    ResolvePaletteButtonStyle(AppTheme), Type.DisplayName,
                    ResolvePaletteButtonStyle(AppTheme).ColorLabel, [&, Id]() {
                        ArmedType = Id;
                        LOG_INFO("Armed placement: {}", Id);
                    }));
            }
            LeftPanelPtr->AddChild(MakeSeparator());
            LeftPanelPtr->AddChild(MakeLabel("Entities", AppTheme.ColorTextSecondary));
            for (const auto& Entity : Scene.Entities) {
                const bool      Selected = (Entity.Id == SelectedId);
                const std::string Id     = Entity.Id;
                auto Btn = MakeButton(ResolveListButtonStyle(AppTheme), Entity.Name,
                                      Selected ? AppTheme.ColorAccent : AppTheme.ColorTextPrimary,
                                      [&, Id]() {
                                          RequestSelectId = Id;
                                          RequestSelect   = true;
                                      });
                if (Selected) {
                    Btn->Style.ColorBackground = AppTheme.ColorSurfaceRaised;
                }
                LeftPanelPtr->AddChild(std::move(Btn));
            }
        };

        // ---- right column: properties for the selected entity ----
        auto RightPanel          = std::make_unique<Box>();
        RightPanel->Style          = ResolvePanelStyle(AppTheme);
        RightPanel->Layout         = LayoutMode::VerticalStack;
        RightPanel->CrossAlignment = CrossAlign::Stretch;
        RightPanel->ChildGap       = AppTheme.SpacingMedium;
        Box* RightPanelPtr = RightPanel.get();

        // Pointers into the current properties widgets, refreshed on rebuild. The two
        // NumericDrags are synced from the entity each frame so a viewport drag is
        // reflected live; the name field is not (it would fight typing).
        NumericDrag* PropX = nullptr;
        NumericDrag* PropY = nullptr;

        auto RebuildProperties = [&]() {
            Focus.Focused = nullptr; // any focused field is about to be destroyed
            PropX = nullptr;
            PropY = nullptr;
            RightPanelPtr->Children.clear();
            RightPanelPtr->AddChild(MakeLabel("Properties", AppTheme.ColorTextSecondary));

            EntityData* Entity = Scene.FindEntity(SelectedId);
            if (Entity == nullptr) {
                RightPanelPtr->AddChild(MakeLabel("No selection", AppTheme.ColorTextDisabled));
                return;
            }

            const SDL_Color Selection{AppTheme.ColorAccent.r, AppTheme.ColorAccent.g,
                                      AppTheme.ColorAccent.b, 120};

            // Name row.
            {
                auto Row = MakeRow();
                Row->AddChild(MakeLabel("Name", AppTheme.ColorTextSecondary));
                auto Field = std::make_unique<TextInput>();
                Field->Style                 = ResolveInputFieldStyle(AppTheme);
                Field->FontBackend           = &FontBackend;
                Field->Font                  = BodyFont;
                Field->ColorText             = AppTheme.ColorTextPrimary;
                Field->ColorCaret            = AppTheme.ColorTextPrimary;
                Field->ColorSelection        = Selection;
                Field->CaretWidthLogical     = AppTheme.BorderWidthDefault;
                Field->PreferredWidthLogical = FieldWidthLogical;
                Field->Focus                 = &Focus;
                Field->Clipboard             = &Window;
                Field->Text                  = Entity->Name;
                Field->OnTextChanged         = [&](const std::string& Text) {
                    if (EntityData* E = Scene.FindEntity(SelectedId)) {
                        E->Name = Text;
                        Scene.MarkDirty();
                        RequestRebuildList = true; // refresh the list label (not this panel)
                    }
                };
                Row->AddChild(std::move(Field));
                RightPanelPtr->AddChild(std::move(Row));
            }

            // Position X / Y rows.
            auto MakeNumeric = [&](const char* LabelText, double Value,
                                   std::function<void(double)> OnChange) -> NumericDrag* {
                auto Row = MakeRow();
                Row->AddChild(MakeLabel(LabelText, AppTheme.ColorTextSecondary));
                auto Drag = std::make_unique<NumericDrag>();
                Drag->Style                 = ResolveInputFieldStyle(AppTheme);
                Drag->FontBackend           = &FontBackend;
                Drag->Font                  = BodyFont;
                Drag->ColorText             = AppTheme.ColorTextPrimary;
                Drag->Value                 = static_cast<float>(Value);
                Drag->Sensitivity           = DragSensitivity;
                Drag->PreferredWidthLogical = FieldWidthLogical;
                Drag->OnValueChanged        = std::move(OnChange);
                NumericDrag* Raw = Drag.get();
                Row->AddChild(std::move(Drag));
                RightPanelPtr->AddChild(std::move(Row));
                return Raw;
            };

            PropX = MakeNumeric("X", Entity->PositionX, [&](float V) {
                if (EntityData* E = Scene.FindEntity(SelectedId)) {
                    E->PositionX = V;
                    Scene.MarkDirty();
                }
            });
            PropY = MakeNumeric("Y", Entity->PositionY, [&](float V) {
                if (EntityData* E = Scene.FindEntity(SelectedId)) {
                    E->PositionY = V;
                    Scene.MarkDirty();
                }
            });
        };

        // ---- pending-edit bookkeeping ----
        auto SnapshotSelection = [&]() {
            if (EntityData* E = Scene.FindEntity(SelectedId)) {
                PendingActive = true;
                PendingId     = SelectedId;
                PendingName   = E->Name;
                PendingX      = E->PositionX;
                PendingY      = E->PositionY;
            } else {
                PendingActive = false;
            }
        };
        // Commits any live change to the selected entity as discrete undoable commands
        // (already applied, so recorded — not executed).
        auto FlushPendingEdit = [&]() {
            if (!PendingActive) {
                return;
            }
            EntityData* E = Scene.FindEntity(PendingId);
            if (E == nullptr) {
                PendingActive = false;
                return;
            }
            if (E->Name != PendingName) {
                auto Cmd = std::make_unique<RenameEntityCommand>(PendingId, PendingName, E->Name);
                LOG_DEBUG("Command recorded: {}", Cmd->Describe());
                Stack.Record(std::move(Cmd));
            }
            // Dead-zone: a click-to-select can jitter the cursor a fraction of a pixel
            // between press and release. Ignore sub-pixel moves so they don't pollute
            // the undo stack with no-op steps; snap the position back to avoid drift.
            constexpr double MoveEpsilon = 0.5;
            if (std::abs(E->PositionX - PendingX) >= MoveEpsilon ||
                std::abs(E->PositionY - PendingY) >= MoveEpsilon) {
                auto Cmd = std::make_unique<MoveEntityCommand>(PendingId, PendingX, PendingY,
                                                               E->PositionX, E->PositionY);
                LOG_DEBUG("Command recorded: {} ({}, {}) -> ({}, {})", Cmd->Describe(), PendingX,
                          PendingY, E->PositionX, E->PositionY);
                Stack.Record(std::move(Cmd));
            } else if (E->PositionX != PendingX || E->PositionY != PendingY) {
                E->PositionX = PendingX;
                E->PositionY = PendingY;
            }
            PendingActive = false;
        };

        auto SetStatus = [&](const std::string& Message, SDL_Color Color) {
            StatusMessage = Message;
            StatusColor   = Color;
        };

        // Serialise the scene to its file via Amanuensis. Never throws; reports IO
        // failure through the status line and Firefly.
        auto SaveScene = [&]() {
            FlushPendingEdit(); // fold any live edit into the history before writing
            const Amanuensis::Value Json = SceneSerialiser::Serialise(Scene);
            if (Amanuensis::Writer::WriteToFile(Json, ScenePath)) {
                Scene.ClearDirty();
                LOG_INFO("Scene saved: {}", ScenePath);
                SetStatus("Saved " + ScenePath, AppTheme.ColorTextSecondary);
            } else {
                LOG_ERROR("Failed to write scene to {}", ScenePath);
                SetStatus("Save failed: " + ScenePath, AppTheme.ColorDestructive);
            }
        };

        // Re-read the scene from disk, replacing the in-memory document. On a missing
        // or malformed file it logs, shows an error, and keeps the current scene — it
        // must never crash or leave a half-loaded document.
        auto ReloadScene = [&]() {
            const Amanuensis::ParseResult Parsed = Amanuensis::Reader::ParseFile(ScenePath);
            if (!Parsed.succeeded) {
                LOG_ERROR("Reload failed: {} at {}:{} — {}", ScenePath, Parsed.error.line,
                          Parsed.error.column, Parsed.error.message);
                SetStatus("Reload failed: " + Parsed.error.message, AppTheme.ColorDestructive);
                return;
            }
            SceneDocument Fresh;
            if (!SceneSerialiser::Deserialise(Parsed.value, Fresh)) {
                LOG_ERROR("Reload failed: {} is not a valid scene object", ScenePath);
                SetStatus("Reload failed: invalid scene", AppTheme.ColorDestructive);
                return;
            }
            Scene = std::move(Fresh);
            Scene.ClearDirty();
            // Reset interaction state that referenced the old document.
            SelectedId.clear();
            ArmedType.clear();
            Dragging      = false;
            PendingActive = false;
            Stack.Clear();
            SceneFramed = false; // re-frame the reloaded scene
            RequestRebuildList  = true;
            RequestRebuildProps = true;
            LOG_INFO("Scene reloaded: '{}' with {} entit{} from {}", Scene.Name,
                     Scene.Entities.size(), Scene.Entities.size() == 1 ? "y" : "ies", ScenePath);
            SetStatus("Reloaded " + ScenePath, AppTheme.ColorTextSecondary);
        };

        // ---- central viewport ----
        auto Viewport = std::make_unique<ViewportWidget>();
        Viewport->Style           = ResolveViewportStyle(AppTheme);
        Viewport->SceneClearColor = AppTheme.ColorBackgroundBase;

        Viewport->OnRenderScene = [&](Penumbra::Render::Renderer& SceneRenderer, SDL_FPoint) {
            for (const auto& Entity : Scene.Entities) {
                const float ScreenX = (static_cast<float>(Entity.PositionX) - Camera.OffsetX) * Camera.Zoom;
                const float ScreenY = (static_cast<float>(Entity.PositionY) - Camera.OffsetY) * Camera.Zoom;
                const float ScreenW = static_cast<float>(Entity.Width) * Camera.Zoom;
                const float ScreenH = static_cast<float>(Entity.Height) * Camera.Zoom;
                SceneRenderer.DrawFilledRect({ScreenX, ScreenY, ScreenW, ScreenH},
                                             ColorForType(Entity.Type));
                if (Entity.Id == SelectedId) {
                    SceneRenderer.DrawRectOutline({ScreenX - 2.0f, ScreenY - 2.0f, ScreenW + 4.0f,
                                                   ScreenH + 4.0f},
                                                  AppTheme.ColorAccent, 2.0f);
                }
            }
        };

        Viewport->OnSceneInput = [&](const InputState& Input, SDL_FRect Content) -> bool {
            const float WorldX = (Input.MousePosition.x - Content.x) / Camera.Zoom + Camera.OffsetX;
            const float WorldY = (Input.MousePosition.y - Content.y) / Camera.Zoom + Camera.OffsetY;

            if (Input.MouseButtonDown[1]) { // middle-drag pan
                Camera.OffsetX -= (Input.MousePosition.x - LastMousePos.x) / Camera.Zoom;
                Camera.OffsetY -= (Input.MousePosition.y - LastMousePos.y) / Camera.Zoom;
            }
            if (Input.MouseWheelDelta != 0.0f) { // wheel zoom, centred on cursor
                const float PreX = (Input.MousePosition.x - Content.x) / Camera.Zoom + Camera.OffsetX;
                const float PreY = (Input.MousePosition.y - Content.y) / Camera.Zoom + Camera.OffsetY;
                Camera.Zoom = std::clamp(Camera.Zoom * (1.0f + Input.MouseWheelDelta * 0.1f), 0.1f, 10.0f);
                Camera.OffsetX = PreX - (Input.MousePosition.x - Content.x) / Camera.Zoom;
                Camera.OffsetY = PreY - (Input.MousePosition.y - Content.y) / Camera.Zoom;
            }

            if (Input.MouseButtonPressedThisFrame[0]) {
                if (!ArmedType.empty()) {
                    RequestPlace     = true;
                    RequestPlaceType = ArmedType;
                    RequestPlaceX    = WorldX;
                    RequestPlaceY    = WorldY;
                } else {
                    const EntityData* Hit = nullptr;
                    for (auto It = Scene.Entities.rbegin(); It != Scene.Entities.rend(); ++It) {
                        const auto& E = *It;
                        if (WorldX >= E.PositionX && WorldX < E.PositionX + E.Width &&
                            WorldY >= E.PositionY && WorldY < E.PositionY + E.Height) {
                            Hit = &E;
                            break;
                        }
                    }
                    if (Hit != nullptr) {
                        RequestSelect   = true;
                        RequestSelectId = Hit->Id;
                        Dragging        = true;
                        DraggedId       = Hit->Id;
                        GrabOffsetX     = WorldX - static_cast<float>(Hit->PositionX);
                        GrabOffsetY     = WorldY - static_cast<float>(Hit->PositionY);
                    } else {
                        RequestClearSelection = true;
                    }
                }
            }

            if (Dragging && Input.MouseButtonDown[0]) {
                if (EntityData* E = Scene.FindEntity(DraggedId)) {
                    E->PositionX = WorldX - GrabOffsetX;
                    E->PositionY = WorldY - GrabOffsetY;
                    Scene.MarkDirty();
                }
            }
            if (Dragging && Input.MouseButtonReleasedThisFrame[0]) {
                Dragging = false;
                FlushPendingEdit();  // records a MoveEntityCommand if the entity moved
                SnapshotSelection(); // re-baseline for the next edit
            }

            LastMousePos = Input.MousePosition;
            return true;
        };

        // Build the initial panels.
        RebuildLeftPanel();
        RebuildProperties();

        InputState Input;
        SDL_FPoint LastViewportSize{-1.0f, -1.0f};
        bool       TextInputActive = false;
        bool       KeepRunning = true;
        while (KeepRunning) {
            KeepRunning = Window.PumpEventsAndBuildInput(Input);

            // ---- layout (manual three-column; Box stacking has no flex/grow) ----
            const SDL_FPoint WindowSize = Window.GetLogicalWindowSize();
            const float M = AppTheme.SpacingMedium;
            const float G = AppTheme.SpacingMedium;
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

            const float Border = AppTheme.BorderWidthDefault;
            const SDL_FPoint ViewportContent{ViewportRect.w - 2.0f * Border,
                                             ViewportRect.h - 2.0f * Border};
            if (ViewportRect.w != LastViewportSize.x || ViewportRect.h != LastViewportSize.y) {
                LOG_TRACE("Viewport resized to {}x{}", ViewportRect.w, ViewportRect.h);
                LastViewportSize = {ViewportRect.w, ViewportRect.h};
            }
            if (!SceneFramed && ViewportContent.x > 1.0f && ViewportContent.y > 1.0f) {
                FrameEntities(Camera, Scene, ViewportContent);
                SceneFramed = true;
                LOG_DEBUG("Framed {} entit{} at zoom {}", Scene.Entities.size(),
                          Scene.Entities.size() == 1 ? "y" : "ies", Camera.Zoom);
            }

            // ---- input pass ----
            // Clicking anywhere clears focus first; a clicked TextInput re-grabs it.
            if (Input.MouseButtonPressedThisFrame[0]) {
                Focus.Focused = nullptr;
            }
            if (!HeaderBar->UpdateInteractionState(Input) &&
                !LeftPanel->UpdateInteractionState(Input) &&
                !RightPanel->UpdateInteractionState(Input)) {
                Viewport->UpdateInteractionState(Input);
            }

            const bool WantTextInput = (Focus.Focused != nullptr);
            if (WantTextInput != TextInputActive) {
                Window.SetTextInputActive(WantTextInput);
                TextInputActive = WantTextInput;
            }

            // Keyboard. While a dirty-reload prompt is up, keys answer it; otherwise
            // Ctrl/Cmd+Z undo, +Shift+Z redo, +S save, +R reload.
            {
                const bool CmdMod = (Input.ModifierState & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
                const bool Shift  = (Input.ModifierState & SDL_KMOD_SHIFT) != 0;
                for (const SDL_Keycode Key : Input.KeysPressedThisFrame) {
                    if (ConfirmingReload) {
                        if (Key == SDLK_Y || Key == SDLK_RETURN) {
                            ConfirmingReload      = false;
                            RequestReloadConfirmed = true;
                        } else if (Key == SDLK_N || Key == SDLK_ESCAPE) {
                            ConfirmingReload = false;
                            SetStatus("Reload cancelled", AppTheme.ColorTextSecondary);
                        }
                        continue;
                    }
                    if (CmdMod) {
                        switch (Key) {
                        case SDLK_Z: (Shift ? RequestRedo : RequestUndo) = true; break;
                        case SDLK_S: RequestSave = true; break;
                        case SDLK_R: RequestReload = true; break;
                        default: break;
                        }
                    }
                }
            }

            // ---- post-input: apply deferred requests (safe to mutate the tree) ----
            if (RequestPlace) {
                FlushPendingEdit();
                const EntityTypeInfo* Info = Schema.Find(RequestPlaceType);
                EntityData NewEntity;
                NewEntity.Type   = RequestPlaceType;
                NewEntity.Width  = Info ? Info->DefaultWidth : 32.0;
                NewEntity.Height = Info ? Info->DefaultHeight : 32.0;
                NewEntity.PositionX = RequestPlaceX - NewEntity.Width * 0.5;  // centre on cursor
                NewEntity.PositionY = RequestPlaceY - NewEntity.Height * 0.5;
                NewEntity.Name  = RequestPlaceType + "_" + std::to_string(++PlacementCounter);
                NewEntity.Layer = Scene.Layers.empty() ? "" : Scene.Layers.front().Id;

                auto Cmd = std::make_unique<PlaceEntityCommand>(std::move(NewEntity));
                const std::string NewId = Cmd->EntityId();
                LOG_INFO("Command: {}", Cmd->Describe());
                Stack.Execute(std::move(Cmd), Scene);

                SelectedId = NewId;
                ArmedType.clear();
                SnapshotSelection();
                RequestRebuildList  = true;
                RequestRebuildProps = true;
                RequestPlace = false;
            }
            if (RequestClearSelection) {
                if (!SelectedId.empty()) {
                    FlushPendingEdit();
                    SelectedId.clear();
                    SnapshotSelection();
                    RequestRebuildList  = true;
                    RequestRebuildProps = true;
                }
                RequestClearSelection = false;
            }
            if (RequestSelect) {
                if (RequestSelectId != SelectedId) {
                    FlushPendingEdit();
                    SelectedId = RequestSelectId;
                    SnapshotSelection();
                    RequestRebuildList  = true;
                    RequestRebuildProps = true;
                    LOG_DEBUG("Selected entity {}", SelectedId);
                }
                RequestSelect = false;
            }
            if (RequestUndo) {
                FlushPendingEdit();
                const std::string Label = Stack.Undo(Scene);
                if (!Label.empty()) {
                    LOG_INFO("Undo: {}", Label);
                    if (Scene.FindEntity(SelectedId) == nullptr) {
                        SelectedId.clear();
                    }
                    SnapshotSelection();
                    RequestRebuildList  = true;
                    RequestRebuildProps = true;
                }
                RequestUndo = false;
            }
            if (RequestRedo) {
                FlushPendingEdit();
                const std::string Label = Stack.Redo(Scene);
                if (!Label.empty()) {
                    LOG_INFO("Redo: {}", Label);
                    if (Scene.FindEntity(SelectedId) == nullptr) {
                        SelectedId.clear();
                    }
                    SnapshotSelection();
                    RequestRebuildList  = true;
                    RequestRebuildProps = true;
                }
                RequestRedo = false;
            }
            if (RequestSave) {
                SaveScene();
                RequestSave = false;
            }
            if (RequestReload) {
                FlushPendingEdit();
                if (Scene.IsDirty()) {
                    ConfirmingReload = true;
                    Focus.Focused    = nullptr; // so Y/N don't type into a field
                } else {
                    ReloadScene();
                }
                RequestReload = false;
            }
            if (RequestReloadConfirmed) {
                ReloadScene();
                RequestReloadConfirmed = false;
            }
            if (RequestRebuildList) {
                RebuildLeftPanel();
                RequestRebuildList = false;
            }
            if (RequestRebuildProps) {
                RebuildProperties();
                RequestRebuildProps = false;
            }

            // Reflect the selected entity's live position in the properties fields
            // (so a viewport drag updates the X/Y readouts).
            if (const EntityData* E = Scene.FindEntity(SelectedId)) {
                if (PropX) PropX->Value = static_cast<float>(E->PositionX);
                if (PropY) PropY->Value = static_cast<float>(E->PositionY);
            }

            // Dirty indicator + status / prompt line in the header.
            DirtyLabel->Text = Scene.IsDirty() ? "*" : "";
            if (ConfirmingReload) {
                StatusLabel->Text      = "Discard unsaved changes and reload?  (Y / N)";
                StatusLabel->ColorText = AppTheme.ColorWarning;
            } else {
                StatusLabel->Text      = StatusMessage;
                StatusLabel->ColorText = StatusColor;
            }

            // ---- draw ----
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
