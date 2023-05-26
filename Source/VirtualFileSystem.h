#pragma once
#include <unordered_map>
#include <filesystem>
#include <functional>
#include <ranges>
#include <variant>

#define VFS_ITERATE_RESOLVE_DIR_LINK 1
#define VFS_ITERATE_RESOLVE_FILE_LINK 2
#define VFS_ITERATE_REPORT_NULL_WALK 4
#define VFS_ITERATE_RESOLVE_ALL (VFS_ITERATE_RESOLVE_DIR_LINK | VFS_ITERATE_RESOLVE_FILE_LINK)

enum EEntryAttribute
{
	eEntryAttribute_None = 0,
	eEntryAttribute_Directory = 1,
	eEntryAttribute_Deleted = 2,
	eEntryAttribute_Null = 4,
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

		Entry* link{};
		std::variant<std::monostate, size_t, std::string> userdata{};

		Entry() = default;
		Entry(int32_t attribute) : attribute(attribute) {}
		Entry(std::string name) : name(std::move(name)) {}
		Entry(Entry* parent) : parent(parent) {}
		Entry(std::string name, Entry* parent) : name(std::move(name)), parent(parent) {}

		template<typename T>
		T& get_data()
		{
			return std::get<T>(userdata);
		}

		template<typename T>
		const T& get_data() const
		{
			return std::get<T>(userdata);
		}

		template<typename T>
		void set_data(T&& data)
		{
			userdata = std::forward<T>(data);
			// return std::get<T>(userdata);
		}

		bool has_data() const
		{
			return !std::holds_alternative<std::monostate>(userdata);
		}

		template<typename T>
		bool has_data() const
		{
			return std::holds_alternative<T>(userdata);
		}

		Entry* get(const char* path, int iterate_flags = VFS_ITERATE_RESOLVE_ALL)
		{
			return get(this, path, iterate_flags);
		}

		Entry* make(const std::filesystem::path& path)
		{
			return make(this, path);
		}

		Entry* make(const char* path)
		{
			return make(this, path);
		}

		void walk(const char* path, const std::function<bool(Entry*)>& callback, int iterate_flags = VFS_ITERATE_RESOLVE_ALL)
		{
			walk(this, path, callback, iterate_flags);
		}

		std::string full_path() const;
		void walk(Entry* root, const char* path, const std::function<bool(Entry*)>& callback, int iterate_flags = VFS_ITERATE_RESOLVE_ALL);
		Entry* get(Entry* root, const char* path, int iterate_flags = VFS_ITERATE_RESOLVE_ALL);
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

		[[nodiscard]] bool is_directory() const
		{
			return attribute & eEntryAttribute_Directory;
		}

		[[nodiscard]] bool is_null() const
		{
			return attribute & eEntryAttribute_Null;
		}
	};

	std::unique_ptr<Entry> root{ new Entry(eEntryAttribute_Directory) };
	void make_link(const char* path, const char* link);
	void make_deleted(const char* path);

	Entry* get_entry(const char* path, int iterate_flags = VFS_ITERATE_RESOLVE_ALL) const;
	Entry* make_entry(const std::filesystem::path& path) const;
	Entry* make_entry(const char* path) const;
};