#include "log.h"
#include <SimpleIni.h>

static std::uint32_t iKeyOpen;

static RE::NiPointer<RE::TESObjectREFR> g_tempAlchemyLab;

void ExecuteConsoleCommand(const std::string& command, RE::TESObjectREFR* targetRef = nullptr)
{
    const auto scriptFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
    const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
    if (script) {
        script->SetCommand(command);
        script->CompileAndRun(targetRef);
        delete script;
    }
}

bool IsSafeToOpenMenu()
{
    auto* ui = RE::UI::GetSingleton();
    if (!ui) {
        return false;
    }

    if (ui->GameIsPaused()) {
        return false;
    }

    if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) ||
        ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME) ||
        ui->IsMenuOpen(RE::MapMenu::MENU_NAME) ||
        ui->IsMenuOpen(RE::JournalMenu::MENU_NAME) ||
        ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME) ||
        ui->IsMenuOpen(RE::Console::MENU_NAME) ||
        ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
        return false;
    }

    return true;
}

RE::TESBoundObject* GetAlchemyLabBaseObject()
{
    auto* base = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESFurniture>(0x000BAD0C, "Skyrim.esm");
    if (!base) {
        SKSE::log::warn("Could not look up alchemy lab base object"sv);
        return nullptr;
    }

    return static_cast<RE::TESBoundObject*>(base);
}

RE::TESObjectREFR* GetOrSpawnTempAlchemyLab()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return nullptr;
    }

    if (g_tempAlchemyLab) {
        g_tempAlchemyLab->MoveTo(player);
        g_tempAlchemyLab->Enable(false);
        return g_tempAlchemyLab.get();
    }

    auto* baseObj = GetAlchemyLabBaseObject();
    if (!baseObj) {
        return nullptr;
    }

    auto newRef = player->PlaceObjectAtMe(baseObj, false);
    if (!newRef) {
        SKSE::log::warn("PlaceObjectAtMe failed to spawn alchemy lab"sv);
        return nullptr;
    }

    g_tempAlchemyLab = newRef;
    return g_tempAlchemyLab.get();
}

void DisableTempAlchemyLab()
{
    if (g_tempAlchemyLab) {
        g_tempAlchemyLab->Disable();
    }
}

class InputHandler : public RE::BSTEventSink<RE::InputEvent*>
{
public:
    static InputHandler* GetSingleton()
    {
        static InputHandler singleton;
        return &singleton;
    }

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
    {
        if (!a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        for (auto event = *a_event; event; event = event->next) {
            if (event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
                continue;
            }

            const auto button = event->AsButtonEvent();
            if (!button || !button->IsDown() || button->device.get() != RE::INPUT_DEVICE::kKeyboard) {
                continue;
            }

            if (!IsSafeToOpenMenu()) {
                return RE::BSEventNotifyControl::kContinue;
            }

            if (button->GetIDCode() == iKeyOpen) {
                if (const auto tempRef = GetOrSpawnTempAlchemyLab()) {
                    std::thread([tempRef]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));

                        SKSE::GetTaskInterface()->AddTask([tempRef]() {
                            ExecuteConsoleCommand("Activate player", tempRef);
                        });
                    }).detach();
                }
                else {
                    SKSE::log::warn("Failed to spawn temporary alchemy lab"sv);
                }
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }

private:
    InputHandler() = default;
};

class MenuWatcher : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
    static MenuWatcher* GetSingleton()
    {
        static MenuWatcher singleton;
        return &singleton;
    }

    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
    {
        if (!a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (a_event->menuName == "Crafting Menu"sv && !a_event->opening) {
            if (g_tempAlchemyLab) {
                std::thread([]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

                    SKSE::GetTaskInterface()->AddTask([]() {
                        DisableTempAlchemyLab();
                    });
                }).detach();
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }

private:
    MenuWatcher() = default;
};

void OnDataLoaded()
{
}

void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
    switch (a_msg->type) {
    case SKSE::MessagingInterface::kDataLoaded:
        OnDataLoaded();
        break;
    case SKSE::MessagingInterface::kPostLoad:
        break;
    case SKSE::MessagingInterface::kPreLoadGame:
        break;
    case SKSE::MessagingInterface::kPostLoadGame:
        break;
    case SKSE::MessagingInterface::kNewGame:
        break;
    case SKSE::MessagingInterface::kInputLoaded:
        RE::BSInputDeviceManager::GetSingleton()->AddEventSink(InputHandler::GetSingleton());
        if (auto* ui = RE::UI::GetSingleton()) {
            ui->AddEventSink(MenuWatcher::GetSingleton());
        }
        break;
    }
}

void LoadSettings()
{
    CSimpleIniA ini;
    ini.SetUnicode();

    const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
    const std::string path = "Data/SKSE/Plugins/" + std::string(plugin->GetName()) + ".ini";
    ini.LoadFile(path.c_str());

    // K key default
    iKeyOpen = static_cast<std::uint32_t>(ini.GetDoubleValue("General", "iKeyOpen", 0x25));
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);
    SetupLog();
    LoadSettings();

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener("SKSE", MessageHandler)) {
        return false;
    }

    return true;
}