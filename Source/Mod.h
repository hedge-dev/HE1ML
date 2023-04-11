#pragma once
#include <filesystem>
#include <string>
#include <vector>

class ModLoader;
typedef void MLAPI ModEvent_t(void* params);
class Mod
{
public:
	std::string id;
	std::string title;
	std::string path;
	std::filesystem::path root;
	std::vector<std::string> include_paths;
	std::vector<HMODULE> modules;
	std::vector<ModEvent_t*> msg_processors;
	ModLoader* loader;

	Mod(ModLoader* in_loader) : loader(in_loader) {}
	bool Init(const std::string& path);
	void InitAdvancedCpk(const char* path);
	void RaiseEvent(const char* name, void* params) const;
	int GetEvents(const char* name, std::vector<ModEvent_t*>& out) const;
	void SendMessageImm(void* message) const;
};