#include "SteamAPI.hpp"

#include "SAR.hpp"

Variable sar_disable_steam_toasts(
	"sar_disable_steam_toasts",
	"0",
	"Disables Steam overlay rendering, including notification toasts.\n");

#ifdef _WIN32
namespace {
using _RenderSteamOverlay = void (__thiscall *)(void *thisptr);
_RenderSteamOverlay RenderSteamOverlay = nullptr;
_RenderSteamOverlay RenderSteamOverlay_Trampoline = nullptr;

void __fastcall RenderSteamOverlay_Hook(void *thisptr, int edx) {
	if (!sar_disable_steam_toasts.GetBool()) {
		RenderSteamOverlay_Trampoline(thisptr);
	}
}
}
#endif

bool SteamAPI::Init() {
	const auto steam_api = Memory::GetModuleHandleByName(this->Name());
	if (!steam_api)
		return false;

#ifdef _WIN32
	const auto renderSteamOverlay = Memory::Scan(
		MODULE("gameoverlayrenderer"),
		"55 8B EC 6A ? 68 ? ? ? ? 64 A1 ? ? ? ? 50 64 89 25 ? ? ? ? 81 EC ? ? ? ? 57");
	if (renderSteamOverlay) {
		MH_HOOK(RenderSteamOverlay, renderSteamOverlay);
	} else {
		console->DevWarning("SAR: Failed to find the Steam overlay renderer.\n");
	}
#endif

	const auto GetHSteamUser = Memory::GetSymbolAddress<void *(*)()>(steam_api, "SteamAPI_GetHSteamUser");
	if (!GetHSteamUser)
		return false;

	const auto FindOrCreateUserInterface = Memory::GetSymbolAddress<void *(*)(void *, const char *)>(steam_api, "SteamInternal_FindOrCreateUserInterface");
	if (FindOrCreateUserInterface) {
		g_timeline = (ISteamTimeline *)FindOrCreateUserInterface(GetHSteamUser(), "STEAMTIMELINE_INTERFACE_V001");
	} else {
		/* FindOrCreateUserInterface remade */
		const auto GetHSteamPipe = Memory::GetSymbolAddress<void *(*)()>(steam_api, "SteamAPI_GetHSteamPipe");
		if (!GetHSteamPipe)
			return false;

#ifdef _WIN32
		auto scanResult = reinterpret_cast<void ***>(Memory::Scan(this->Name(), Offsets::interfaceMgrSig, Offsets::interfaceMgrOff));
		
		if (scanResult && *scanResult && **scanResult) {
			void *interfaceMgr = **scanResult;
			const auto fn = *(ISteamTimeline * (__rescall **)(void *, void *, void *, const char *))(*(uintptr_t *)interfaceMgr + 48);
			g_timeline = fn(interfaceMgr, GetHSteamUser(), GetHSteamPipe(), "STEAMTIMELINE_INTERFACE_V001");
		}
#else
		// TODO: Linux mods support
		return false;
#endif
	}
	if (!g_timeline)
		return false;

	g_timeline->SetTimelineGameMode(k_ETimelineGameMode_Menus);

	SteamUser = Memory::GetSymbolAddress<ISteamUser *(*)()>(steam_api, "SteamAPI_SteamUser_v021");

	return this->hasLoaded = this->g_timeline;
}
void SteamAPI::Shutdown() {
#ifdef _WIN32
	MH_UNHOOK(RenderSteamOverlay);
	RenderSteamOverlay = nullptr;
	RenderSteamOverlay_Trampoline = nullptr;
#endif
}

SteamAPI *steam;
