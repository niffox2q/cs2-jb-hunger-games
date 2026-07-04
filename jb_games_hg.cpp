#include "jb_games_hg.h"
#include <random>
#include <cstdio>
#include <algorithm>

#define MAX_PLAYERS 64

#define CS_TEAM_NONE 0
#define CS_TEAM_SPECTATOR 1
#define CS_TEAM_T 2
#define CS_TEAM_CT 3



jb_games_hg g_jb_games_hg;
PLUGIN_EXPOSE(jb_games_hg, g_jb_games_hg);

// SYSTEM API`s
IVEngineServer2* engine = nullptr;
CGlobalVars* gpGlobals = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;

// API
IUtilsApi* utils;
IMenusApi* menus_api;
IPlayersApi* players_api;
IJailbreakApi* jailbreak_api;

// VARS

bool bHungerGamesEnabled = false;

std::map<std::string, std::string> phrases;




// =========================================
// CONFIG VARS
// =========================================
bool g_bEnableGuardGodMode;
bool g_bGuardNoDamage;

//==========================================
// HELPERS
//==========================================
ConVarRefAbstract* FindConVar(const char* sCvarName)
{
    return new ConVarRefAbstract(g_pCVar->FindConVar(sCvarName));
}



// =========================================
// CONFIGS 
// =========================================

void LoadConfig() {
    KeyValues* config = new KeyValues("Config");
    const char* path = "addons/configs/Jailbreak/hunger_games.ini";
    if (!config->LoadFromFile(g_pFullFileSystem, path)) {
        utils->ErrorLog("%s Failed to load: %s",g_PLAPI->GetLogTag(), path);
        delete config;
        return;
        
    }

    g_bEnableGuardGodMode = config->GetBool("GuardGodmode",false);
    g_bGuardNoDamage      = config->GetBool("GuardNoDamage",true);
    

    delete config;
}

void LoadTranslations() {
    phrases.clear();
    KeyValues* g_kvPhrases = new KeyValues("Phrases");
    const char *pszPath = "addons/translations/jailbreak.phrases.txt";

    if (!g_kvPhrases->LoadFromFile(g_pFullFileSystem, pszPath))
    {
        utils->ErrorLog("%s Failed to load %s", g_PLAPI->GetLogTag(), pszPath);
        delete g_kvPhrases;
        return;
    }

    const char* language = utils->GetLanguage();

    for (KeyValues *pKey = g_kvPhrases->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey()) {
        phrases[std::string(pKey->GetName())] = std::string(pKey->GetString(language));
    }
    delete g_kvPhrases;
}

const char* GetTranslation(const char* key) {
    auto it = phrases.find(key);
    if (it == phrases.end()) return key;
    else return it->second.c_str();
}

void PrintSlotPrefixed(int iSlot, const char* content) {
    if (!content || content[0] == '\0') return;
    char buf[512];
    g_SMAPI->Format(buf, sizeof(buf), "%s %s", GetTranslation("Prefix"), content);
    utils->PrintToChat(iSlot, buf);
}

void PrintAllPrefixed(const char* content) {
    if (!content || content[0] == '\0') return;
    char buf[512];
    g_SMAPI->Format(buf, sizeof(buf), "%s %s", GetTranslation("Prefix"), content);
    utils->PrintToChatAll(buf);
}



// =========================================
// OTHER
// =========================================

void GiveAllTWeapons(std::string sWeaponName){
    for (int i = 0; i < MAX_PLAYERS; i++){
        auto pController = CCSPlayerController::FromSlot(i);
        if (!pController || pController->GetTeam() != CS_TEAM_T) continue;
        auto pPawn = pController->GetPlayerPawn();
        if (!pPawn || !pPawn->IsAlive()) continue;

        players_api->RemoveWeapons(i);
        auto ItemService = pPawn->m_pItemServices.Get();
        if (ItemService) {
            ItemService->GiveNamedItem("weapon_knife");
            ItemService->GiveNamedItem(sWeaponName.c_str());
        }
    }
}

void ClearAllWeapons(){
    for (int i = 0; i < MAX_PLAYERS; i++){
        auto pController = CCSPlayerController::FromSlot(i);
        if (!pController || pController->GetTeam() != CS_TEAM_T) continue;
        auto pPawn = pController->GetPlayerPawn();
        if (!pPawn || !pPawn->IsAlive()) continue;

        players_api->RemoveWeapons(i);
        auto ItemService = pPawn->m_pItemServices.Get();
        if (ItemService) {
            ItemService->GiveNamedItem("weapon_knife");
        }
    }
}

void TurnOffHungerGames(){
    bHungerGamesEnabled = false;
    ClearAllWeapons();
    ConVarRefAbstract* FFCvar = FindConVar("mp_teammates_are_enemies");
    if (FFCvar) {
        FFCvar->SetBool(false);
        delete FFCvar;
    }
}

void TurnOffMenu(int iSlot){
    if (jailbreak_api->GetWarden() != iSlot) return;
    Menu hMenu;
    menus_api->SetTitleMenu(hMenu,GetTranslation("HungerGames_YouSureTurnOff"));
    menus_api->AddItemMenu(hMenu,"yes",GetTranslation("HungerGames_YesButton"),ITEM_DEFAULT);
    menus_api->AddItemMenu(hMenu,"no",GetTranslation("HungerGames_NoButton"),ITEM_DEFAULT);
    menus_api->SetExitMenu(hMenu,true);
    menus_api->SetCallback(hMenu,[](const char* szBack, const char* szFront, int iItem, int iSlot){
        if (!szBack || szBack[0] == '\0') return;
        if (strcmp(szBack,"yes") == 0) {
            TurnOffHungerGames();
            menus_api->ClosePlayerMenu(iSlot);
            return;
        } else {
            menus_api->ClosePlayerMenu(iSlot);
        }
    }); 
    menus_api->DisplayPlayerMenu(hMenu,iSlot,true,true);
}


void OnHungyGamesButton(int iSlot){
    if (jailbreak_api->GetWarden() != iSlot) return;
    if (bHungerGamesEnabled) {
        TurnOffMenu(iSlot);
        return;
    }
    Menu hMenu;
    menus_api->SetTitleMenu(hMenu,GetTranslation("HungerGames_MenuTitle"));

    menus_api->AddItemMenu(hMenu,"weapon_awp","AWP",ITEM_DEFAULT);
    menus_api->AddItemMenu(hMenu,"weapon_ak47","AK-47",ITEM_DEFAULT);
    menus_api->AddItemMenu(hMenu,"weapon_m4a1","M4A4",ITEM_DEFAULT);
    menus_api->AddItemMenu(hMenu,"weapon_m4a1_silencer","M4A1-S",ITEM_DEFAULT);

    menus_api->SetExitMenu(hMenu,true);
    menus_api->SetCallback(hMenu,[](const char* szBack, const char* szFront, int iItem, int iSlot){
        if (jailbreak_api->GetWarden() != iSlot) return;
        if (strcmp(szBack,"") == 0) return;
        if (strcmp(szBack,"exit") == 0) return;

        bHungerGamesEnabled = true;
        ConVarRefAbstract* CVar = FindConVar("mp_teammates_are_enemies");
        if (CVar) {
            GiveAllTWeapons(szBack);
            CVar->SetBool(true);
        }

        PrintAllPrefixed(GetTranslation("HungerGames_Start"));
        delete CVar;
        
        menus_api->ClosePlayerMenu(iSlot);
    }); 

    menus_api->DisplayPlayerMenu(hMenu,iSlot,true,true);
}



CGameEntitySystem* GameEntitySystem() {
    return utils ? utils->GetCGameEntitySystem() : nullptr;
}



void StartupServer() {
    g_pGameEntitySystem = GameEntitySystem();
    g_pEntitySystem = utils->GetCEntitySystem();
    gpGlobals = utils->GetCGlobalVars();
}

bool jb_games_hg::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late) {
    PLUGIN_SAVEVARS();

    GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameEntities, ISource2GameEntities, SOURCE2GAMEENTITIES_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkSystem, INetworkSystem, NETWORKSYSTEM_INTERFACE_VERSION);

    ConVar_Register(FCVAR_SERVER_CAN_EXECUTE | FCVAR_GAMEDLL);
    g_SMAPI->AddListener(this, this);

    return true;
}

CON_COMMAND_F(mm_hungergames_reload,"Ooo",FCVAR_CLIENT_CAN_EXECUTE){
    LoadConfig();
    LoadTranslations();
    META_CONPRINTF("Success\n");
}



void jb_games_hg::AllPluginsLoaded() {
    int ret;
    utils = (IUtilsApi*)g_SMAPI->MetaFactory(Utils_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED) {
        META_CONPRINTF("%s | Missing UTILS plugin.",g_PLAPI->GetLogTag());
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }


    menus_api = (IMenusApi*)g_SMAPI->MetaFactory(Menus_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED) {
        META_CONPRINTF("%s | Missing UTILS plugin.",g_PLAPI->GetLogTag());
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }

    players_api = (IPlayersApi*)g_SMAPI->MetaFactory(PLAYERS_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED) {
        META_CONPRINTF("%s | Missing UTILS plugin.",g_PLAPI->GetLogTag());
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }

    jailbreak_api =(IJailbreakApi*)g_SMAPI->MetaFactory(JAILBREAK_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED) {
        META_CONPRINTF("%s | Missing Jailbreak Core plugin.",g_PLAPI->GetLogTag());
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }


    LoadTranslations();
    LoadConfig();

    jailbreak_api->RegisterGameFeature(g_PLID,"hungrygames",GetTranslation("Game_HungryGames"),OnHungyGamesButton);
    utils->HookEvent(g_PLID,"round_start",[](const char* szName, IGameEvent* pEvent, bool bDontBroadcast){
        if (bHungerGamesEnabled) {
            ConVarRefAbstract* FFCvar = FindConVar("mp_teammates_are_enemies");
            if (FFCvar) {
                FFCvar->SetBool(false);
            }
            bHungerGamesEnabled = false;
            delete FFCvar;
        }

    }); 

    utils->HookOnTakeDamagePre(g_PLID, [](int iSlot, CTakeDamageInfo *pInfo) {
        if (!bHungerGamesEnabled) return true;

        if (g_bEnableGuardGodMode) {
            auto pController = CCSPlayerController::FromSlot(iSlot);
            if (pController && pController->GetTeam() == CS_TEAM_CT) return false;
        }

        if (g_bGuardNoDamage) {
            auto AttackerHandle = pInfo->m_hAttacker.Get();
            if (AttackerHandle.IsValid()) {
                auto attacker = AttackerHandle.Get();
                if (attacker) {
                    auto AttackerEntity = (CBaseEntity*)attacker;
                    if (AttackerEntity) {
                        if (AttackerEntity->GetTeam() == CS_TEAM_CT) return false;
                    }
                }
            }
        }
        return true;
    });

    jailbreak_api->OnGiveLR(g_PLID,[](int iSlot){
        if (bHungerGamesEnabled) {
            ClearAllWeapons();
            ConVarRefAbstract* FFCvar = FindConVar("mp_teammates_are_enemies");
            if (FFCvar) {
                FFCvar->SetBool(false);
            }
            bHungerGamesEnabled = false;
            delete FFCvar;
            auto pController = CCSPlayerController::FromSlot(iSlot);
            if (!pController) return;
            char msg[256];
            g_SMAPI->Format(msg,sizeof(msg),GetTranslation("HungerGames_WinnerDetected"),pController->GetPlayerName());
            PrintAllPrefixed(msg);
        }
    });

    utils->StartupServer(g_PLID, StartupServer);

    
    
}

bool jb_games_hg::Unload(char* error, size_t maxlen) {
    jailbreak_api->ClearAllPluginHooks(g_PLID);
    utils->ClearAllHooks(g_PLID);
    ConVar_Unregister();

   
    return true;
}

const char* jb_games_hg::GetAuthor() { return "niffox"; }
const char* jb_games_hg::GetDate() { return __DATE__; }
const char* jb_games_hg::GetDescription() { return "[JB] Hunger Games"; }
const char* jb_games_hg::GetLicense() { return "Private"; }
const char* jb_games_hg::GetLogTag() { return "[JB] Hunger Games"; }
const char* jb_games_hg::GetName() { return "[JB] Hunger Games"; }
const char* jb_games_hg::GetURL() { return "https://t.me/niffox_2q"; }
const char* jb_games_hg::GetVersion() { return "1.0.1"; }