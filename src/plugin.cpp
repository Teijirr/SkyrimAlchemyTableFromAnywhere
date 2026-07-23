#include "log.h"
#include <SimpleIni.h>

static std::uint32_t iKeyOpen;

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

RE::TESObjectREFR* GetAlchemyTableRef()
{
	// Dragonsreach AlchemyLab
	auto alchemyTable = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectREFR>(0x0010D982, "Skyrim.esm");
	return alchemyTable;
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

			if (button->GetIDCode() == iKeyOpen) {
				if (const auto targetRef = GetAlchemyTableRef()) {
					ExecuteConsoleCommand("Activate player", targetRef);
				}
				else {
					SKSE::log::warn("No target ref set yet"sv);
				}
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}

private:
	InputHandler() = default;
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

	// X key default
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