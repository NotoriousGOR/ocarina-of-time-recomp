#include <RmlUi/Core.h>

#include <iostream>
#include <string>

class NullRenderer : public Rml::RenderInterface {
public:
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override { return 1; }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i& size, const Rml::String&) override {
        size = {1, 1};
        return 1;
    }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override { return 1; }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};

struct SelectionListener : Rml::EventListener {
    bool& selected;
    Rml::DataModelHandle& model;
    bool value;

    SelectionListener(bool& selected, Rml::DataModelHandle& model, bool value)
        : selected(selected), model(model), value(value) {}

    void ProcessEvent(Rml::Event&) override {
        selected = value;
        model.DirtyVariable("majora_selected");
    }
};

struct Listeners : Rml::EventListenerInstancer {
    bool selected = true;
    Rml::DataModelHandle model;
    SelectionListener oot{selected, model, false};
    SelectionListener majora{selected, model, true};

    Rml::EventListener* InstanceEventListener(const Rml::String& value, Rml::Element*) override {
        if (value == "select_oot") return &oot;
        if (value == "select_majora") return &majora;
        return nullptr;
    }
};

static Rml::Element* visible_element(Rml::ElementDocument* document, const char* selector) {
    Rml::ElementList elements;
    document->QuerySelectorAll(elements, selector);
    for (auto* element : elements) {
        if (element->IsVisible(true)) return element;
    }
    return nullptr;
}

static bool click_title(Rml::Context* context, Rml::ElementDocument* document, const char* selector) {
    auto* button = visible_element(document, selector);
    if (!button) {
        std::cerr << "No visible title: " << selector << '\n';
        return false;
    }
    auto pos = button->GetAbsoluteOffset(Rml::BoxArea::Border);
    auto size = button->GetBox().GetSize(Rml::BoxArea::Border);
    int x = static_cast<int>(pos.x + size.x / 2);
    int y = static_cast<int>(pos.y + size.y / 2);
    auto* hit = context->GetElementAtPoint({static_cast<float>(x), static_cast<float>(y)});
    std::cerr << selector << " at " << x << "," << y << " hit "
              << (hit ? hit->GetTagName() : "null") << " class "
              << (hit ? hit->GetClassNames() : "") << '\n';
    context->ProcessMouseMove(x, y, 0);
    context->ProcessMouseButtonDown(0, 0);
    context->ProcessMouseButtonUp(0, 0);
    context->Update();
    return true;
}

static bool majora_menu_has_no_dots(Rml::Context* context, Rml::ElementDocument* document) {
    Rml::ElementList buttons;
    document->QuerySelectorAll(buttons, "button.menu-list-item--right");
    bool found_visible = false;
    for (auto* button : buttons) {
        if (!button->IsVisible(true)) continue;
        found_visible = true;
        auto pos = button->GetAbsoluteOffset(Rml::BoxArea::Border);
        auto size = button->GetBox().GetSize(Rml::BoxArea::Border);
        context->ProcessMouseMove(static_cast<int>(pos.x + size.x / 2), static_cast<int>(pos.y + size.y / 2), 0);
        context->Update();
        auto* bullet = button->QuerySelector(".menu-list-item__bullet");
        if (bullet && bullet->IsVisible(true) && bullet->GetComputedValues().opacity() > 0) return false;
    }
    return found_visible;
}

static bool menu_on_right(Rml::ElementDocument* document, bool expected) {
    Rml::ElementList buttons;
    document->QuerySelectorAll(buttons, "button[onclick='start_game']");
    Rml::Element* visible = nullptr;
    for (auto* button : buttons) {
        if (button->IsVisible(true)) {
            if (visible) return false;
            visible = button;
        }
    }
    if (!visible) return false;
    for (auto* parent = visible; parent; parent = parent->GetParentNode()) {
        if (parent->IsClassSet("launcher__vertical-split--right")) return expected;
    }
    return !expected;
}

static bool background_matches(Rml::ElementDocument* document, bool majora) {
    auto* mm = document->QuerySelector(".launcher__background-mm");
    auto* oot = document->QuerySelector(".launcher__background-oot");
    return mm && oot && mm->GetAttribute<Rml::String>("src", "") == "mm-clipped.svg" &&
           mm->IsVisible(true) == majora && oot->IsVisible(true) != majora;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Expected path to assets directory\n";
        return 1;
    }
    const std::string assets = argv[1];
    NullRenderer renderer;
    Listeners listeners;
    Rml::SetRenderInterface(&renderer);
    if (!Rml::Initialise()) return 1;
    Rml::Factory::RegisterEventListenerInstancer(&listeners);
    for (const char* font : {"LatoLatin-Regular.ttf", "LatoLatin-Bold.ttf", "ChiaroNormal.otf", "ChiaroBold.otf"}) {
        if (!Rml::LoadFontFace(assets + "/" + font)) return 1;
    }
    auto* context = Rml::CreateContext("launcher-test", {1600, 900});
    if (!context) return 1;
    context->SetDensityIndependentPixelRatio(900.0f / 1080.0f);
    auto constructor = context->CreateDataModel("launcher_model");
    std::string version = "test";
    if (!constructor.Bind("majora_selected", &listeners.selected) || !constructor.Bind("version_number", &version)) return 1;
    listeners.model = constructor.GetModelHandle();
    auto* document = context->LoadDocument(assets + "/launcher.rml");
    if (!document) return 1;
    document->Show();
    context->Update();

    bool ok = menu_on_right(document, true) && background_matches(document, true) &&
              majora_menu_has_no_dots(context, document);
    for (const auto dimensions : {Rml::Vector2i{1600, 900}, Rml::Vector2i{1024, 768}}) {
        context->SetDimensions(dimensions);
        context->SetDensityIndependentPixelRatio(dimensions.y / 1080.0f);
        context->Update();
        ok = ok && click_title(context, document, "button[onclick='select_oot']") && !listeners.selected &&
             menu_on_right(document, false) && background_matches(document, false);
        ok = ok && click_title(context, document, "button[onclick='select_majora']") && listeners.selected &&
             menu_on_right(document, true) && background_matches(document, true) &&
             majora_menu_has_no_dots(context, document);
    }
    Rml::Shutdown();
    if (!ok) {
        std::cerr << "Title switching, menu/background visibility, or Majora menu dots check failed\n";
        return 1;
    }
    std::cout << "Both game titles switched successfully\n";
    return 0;
}
