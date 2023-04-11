#pragma once

#define MLAPI __cdecl
struct MLUpdateInfo
{
	void* device;
};

#ifdef MODLOADER_IMPLEMENTATION
#define LOG(MSG, ...) { printf("[HE1ML] " MSG "\n", __VA_ARGS__); }

#include <string>
#include <memory>
#include <vector>
#include "Mod.h"
#include "FileBinder.h"
#include "VirtualFileSystem.h"

class Mod;
class ModLoader
{
public:
	std::string config_path{};
	std::string database_path{};
	std::string root_path{};
	std::unique_ptr<FileBinder> binder{ new FileBinder()};
	VirtualFileSystem* vfs{ &binder->vfs };
	std::vector<std::unique_ptr<Mod>> mods{};
	std::vector<ModEvent_t*> update_handlers{};
	MLUpdateInfo update_info{};

	void Init(const char* configPath);
	void LoadDatabase(const std::string& databasePath, bool append = false);
	bool RegisterMod(const std::string& path);
	void BroadcastMessageImm(void* message) const;
	void OnUpdate();
};
#endif