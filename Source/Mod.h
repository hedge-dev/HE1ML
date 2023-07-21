#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "CpkAdvancedConfig.h"

class ModLoader;
typedef void ML_API ModEvent_t(void* params);
typedef void ML_API ModMessageProc_t(size_t id, void* params);
class Mod
{
public:
	std::string id;
	std::string title;
	std::string path;
	std::string save_file;
	std::filesystem::path root;
	std::vector<std::string> include_paths;
	std::vector<HMODULE> modules;
	std::vector<ModMessageProc_t*> msg_processors;
	std::vector<CpkAdvancedConfig> cpk_configs;
	ModLoader* loader;
	int bind_priority = 0;

	Mod(ModLoader* in_loader) : loader(in_loader) {}
	bool Load(const std::string& path);
	void LoadAdvancedCpk(const char* path);
	void Init(int bind_priority = 0);
	void RaiseEvent(const char* name, void* params) const;
	int GetEvents(const char* name, std::vector<ModEvent_t*>& out) const;
	void SendMessageImm(size_t id, void* data) const;
	int BindFile(const char* source, const char* destination, int priority = 0) const;
	int BindDirectory(const char* source, const char* destination, int priority = 0) const;
};