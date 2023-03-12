#pragma once
#include <unordered_map>
#include <filesystem>
#include <functional>
#include <ranges>

enum EEntryAttribute
{
	eEntryAttribute_None = 0,
	eEntryAttribute_Directory = 1,
	eEntryAttribute_Deleted = 2,
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

		std::string link{};
		size_t userdata{};

		Entry() = default;
		Entry(std::string in_name) : name(std::move(in_name)) {}
		Entry(Entry* in_parent) : parent(in_parent) {}
		Entry(std::string in_name, Entry* in_parent) : name(std::move(in_name)), parent(in_parent) {}

		Entry* get(const char* path, bool resolve_link = true)
		{
			return get(this, path, resolve_link);
		}

		Entry* make(const std::filesystem::path& path)
		{
			return make(this, path);
		}

		Entry* make(const char* path)
		{
			return make(this, path);
		}

		void walk(const char* path, const std::function<bool(Entry*)>& callback, bool resolve_link = true)
		{
			walk(this, path, callback, resolve_link);
		}

		std::string full_path() const;
		void walk(Entry* root, const char* path, const std::function<bool(Entry*)>& callback, bool resolve_link = true);
		Entry* get(Entry* root, const char* path, bool resolve_link = true);
		Entry* make(Entry* root, const std::filesystem::path& path);
		Entry* make(Entry* root, const char* path);

		void erase(const Entry* child)
		{
			const auto& it = children.find(child->name);
			if (it != children.end())
			{
				children.erase(it);
			}
		}

		void disown(const Entry* child)
		{
			const auto& it = children.find(child->name);
			if (it != children.end())
			{
				it->second.release();  // NOLINT(bugprone-unused-return-value)
				children.erase(it);
			}
		}

		void foreach(const std::function<bool(Entry*)>& callback) const
		{
			for (const auto& val : children | std::views::values)
			{
				if (callback(val.get()))
				{
					val->foreach(callback);
				}
			}
		}

		[[nodiscard]] bool is_root() const
		{
			return this == nullptr || parent == nullptr;
		}
	};

	std::unique_ptr<Entry> root{ new Entry() };
	void make_link(const char* path, const std::string& link);
	void make_link(const char* path, const char* link);
	void make_deleted(const char* path);

	Entry* get_entry(const char* path, bool resolve_link = true) const;
	Entry* make_entry(const std::filesystem::path& path) const;
	Entry* make_entry(const char* path) const;
};