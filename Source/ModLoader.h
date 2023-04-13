#pragma once

#define ML_API __cdecl
struct MLUpdateInfo
{
	void* device;
};

#define ML_API_VERSION 1
struct ModLoaderAPI_t;
struct CommonLoaderAPI;

#ifdef MODLOADER_IMPLEMENTATION
namespace v0
{
#endif
	struct Mod_t
	{
		const char* Name;
		const char* Path;
		const char* ID;
		void* pImpl;
	};

	// stdc++ compatability
	// We can't use std::vector because it's not guaranteed to be ABI compatible
	struct ModList_t
	{
		const Mod_t** first;
		const Mod_t** last;
		const Mod_t** capacity;

#ifdef MODLOADER_IMPLEMENTATION
		ModList_t() : first(nullptr), last(nullptr), capacity(nullptr) {}
		ModList_t(const Mod_t** start, const Mod_t** fin) : first(start), last(fin), capacity(fin) {}
#endif

#ifdef __cplusplus
		const Mod_t** begin() const { return first; }
		const Mod_t** end() const { return last; }

		const Mod_t*& operator[](size_t i) const { return first[i]; }
		size_t size() const { return last - first; }
#endif
	};

	struct ModInfo_t
	{
		ModList_t* ModList;
		Mod_t* CurrentMod;
		const ModLoaderAPI_t* API;
		void* Reserved[2];
	};

#ifdef MODLOADER_IMPLEMENTATION
}
#endif

#define DECLARE_API_FUNC(RETURN_TYPE, NAME, ...) RETURN_TYPE (ML_API *NAME)(__VA_ARGS__) = nullptr;

#ifdef MODLOADER_IMPLEMENTATION
typedef v0::Mod_t Mod_t;
#endif

struct ModLoaderAPI_t
{
	DECLARE_API_FUNC(unsigned int, GetVersion);
	DECLARE_API_FUNC(const CommonLoaderAPI*, GetCommonLoader);
	DECLARE_API_FUNC(const Mod_t*, FindMod, const char* id);
	DECLARE_API_FUNC(void, SendMessageImm, const Mod_t* mod, size_t id, void* data);
	DECLARE_API_FUNC(void, SendMessageToLoader, size_t id, void* data);
	DECLARE_API_FUNC(int, BindFile, const char* path, const char* destination);
	DECLARE_API_FUNC(int, BindDirectory, const char* path, const char* destination);
	DECLARE_API_FUNC(void, SetSaveFile, const char* path);
};

#undef DECLARE_API_FUNC

#ifdef MODLOADER_IMPLEMENTATION
#define LOG(MSG, ...) { printf("[HE1ML] " MSG "\n", __VA_ARGS__); }

#include <string>
#include <memory>
#include <vector>
#include "Mod.h"
#include "FileBinder.h"
#include "VirtualFileSystem.h"

extern ModLoaderAPI_t g_ml_api;

class Mod;
class ModLoader
{
public:
	std::string config_path{};
	std::string database_path{};
	std::string root_path{};
	std::string save_file{ "hedgehog.sav" };
	bool save_redirection{ false };
	bool save_read_through{ true };

	std::unique_ptr<FileBinder> binder{ new FileBinder() };
	VirtualFileSystem* vfs{ &binder->vfs };
	std::vector<std::unique_ptr<Mod>> mods{};
	std::vector<std::unique_ptr<v0::Mod_t>> mod_handles{};

	std::vector<ModEvent_t*> update_handlers{};
	MLUpdateInfo update_info{};

	void Init(const char* configPath);
	void LoadDatabase(const std::string& databasePath, bool append = false);
	bool RegisterMod(const std::string& path);
	void BroadcastMessageImm(size_t id, void* data) const;
	void OnUpdate();
	void ProcessMessage(size_t id, void* data);

	void SetUseSaveRedirection(bool value)
	{
		save_redirection = value;
	}

	void SetSaveFile(const char* path);
};
#endif