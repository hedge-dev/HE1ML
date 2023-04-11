#pragma once

#define MLAPI __cdecl
struct MLUpdateInfo
{
	void* device;
};

#ifdef MODLOADER_IMPLEMENTATION
namespace v0
{
#endif
	struct Mod_t
	{
		const char* Name;
		const char* Path;
		const char* ID;
	};

	// stdc++ compatability
	// We can't use std::vector because it's not guaranteed to be ABI compatible
	struct ModList_t
	{
		const Mod_t* start;
		const Mod_t* fin;
		const Mod_t* capacity;

#ifdef MODLOADER_IMPLEMENTATION
		ModList_t() : start(nullptr), fin(nullptr), capacity(nullptr) {}
		ModList_t(const Mod_t* start, const Mod_t* fin) : start(start), fin(fin), capacity(fin) {}
#endif

#ifdef __cplusplus
		const Mod_t* begin() const { return start; }
		const Mod_t* end() const { return fin; }

		const Mod_t& operator[](size_t i) const { return start[i]; }
		size_t size() const { return fin - start; }
#endif
	};

	struct ModInfo_t
	{
		ModList_t* ModList;
		Mod_t* CurrentMod;
		int Version;
		void* Reserved[2];
	};

#ifdef MODLOADER_IMPLEMENTATION
}
#endif

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
	std::unique_ptr<FileBinder> binder{ new FileBinder() };
	VirtualFileSystem* vfs{ &binder->vfs };
	std::vector<std::unique_ptr<Mod>> mods{};
	std::vector<v0::Mod_t> mod_handles{};

	std::vector<ModEvent_t*> update_handlers{};
	MLUpdateInfo update_info{};

	void Init(const char* configPath);
	void LoadDatabase(const std::string& databasePath, bool append = false);
	bool RegisterMod(const std::string& path);
	void BroadcastMessageImm(void* message) const;
	void OnUpdate();
};
#endif