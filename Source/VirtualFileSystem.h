#pragma once
#include <unordered_map>

enum EEntryAttribute
{
	eEntryAttribute_None      = 0,
	eEntryAttribute_Directory = 1,
	eEntryAttribute_Deleted   = 2,
};

class VirtualFileSystem
{
public:
	struct Entry
	{
		std::string name{};
		Entry* parent{};
		std::unordered_map<std::string, std::unique_ptr<Entry>> children{};
		int32_t attribute{};

		std::string link;

		Entry() = default;
		Entry(std::string in_name) : name(std::move(in_name)) {}
		Entry(Entry* in_parent) : parent(in_parent) {}
		Entry(std::string in_name, Entry* in_parent) : name(std::move(in_name)), parent(in_parent) {}
	};

	std::unique_ptr<Entry> root{ new Entry() };
	void make_link(const char* path, const char* link);
	const std::string& get_link(const char* path) const;
};