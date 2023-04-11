#pragma once
#include "ini.h"

class IniProperty
{
public:
	ini_t* ini;
	int section_id;
	int property_id;

	IniProperty(ini_t* in_ini, int in_section_id, int in_property_id) : ini(in_ini), section_id(in_section_id), property_id(in_property_id)
	{

	}

	[[nodiscard]] const char* name() const
	{
		return ini_property_name(ini, section_id, property_id);
	}

	[[nodiscard]] const char* value() const
	{
		return ini_property_value(ini, section_id, property_id);
	}

	[[nodiscard]] bool valid() const
	{
		return section_id != INI_NOT_FOUND && property_id != INI_NOT_FOUND;
	}
};

class IniSection
{
public:
	struct iterator
	{
		ini_t* ini;
		int section_id;
		int index;

		iterator(ini_t* in_ini, int in_section_id, int in_index) : ini(in_ini), section_id(in_section_id), index(in_index)
		{

		}

		iterator& operator++()
		{
			index++;
			return *this;
		}

		iterator operator++(int)
		{
			iterator tmp = *this;
			operator++();
			return tmp;
		}

		bool operator==(const iterator& rhs) const
		{
			return index == rhs.index;
		}

		bool operator!=(const iterator& rhs) const
		{
			return index != rhs.index;
		}

		IniProperty operator*() const
		{
			return { ini, section_id, index };
		}
	};

	ini_t* ini;
	int section_id{};

	IniSection(ini_t* in_ini, int id) : ini(in_ini), section_id(id)
	{

	}

	const char* name() const
	{
		return ini_section_name(ini, section_id);
	}

	[[nodiscard]] size_t size() const
	{
		return ini_property_count(ini, section_id);
	}

	[[nodiscard]] iterator begin() const
	{
		return { ini, section_id, 0 };
	}

	[[nodiscard]] iterator end() const
	{
		return { ini, section_id, ini_property_count(ini, section_id) };
	}

	[[nodiscard]] bool valid() const
	{
		return section_id != INI_NOT_FOUND;
	}

	const char* operator[](int index) const
	{
		return ini_property_value(ini, section_id, index);
	}

	const char* operator[](const char* key) const
	{
		const auto property = ini_find_property(ini, section_id, key, 0);
		if (property == INI_NOT_FOUND)
			return "";

		return ini_property_value(ini, section_id, property);
	}
};

class Ini
{
public:
	class iterator
	{
	public:
		ini_t* ini;
		int index;

		iterator(ini_t* in_ini, int in_index) : ini(in_ini), index(in_index)
		{

		}

		iterator& operator++()
		{
			index++;
			return *this;
		}

		iterator operator++(int)
		{
			iterator tmp = *this;
			operator++();
			return tmp;
		}

		bool operator==(const iterator& rhs) const
		{
			return index == rhs.index;
		}

		bool operator!=(const iterator& rhs) const
		{
			return index != rhs.index;
		}

		IniSection operator*() const
		{
			return { ini, index };
		}
	};

	ini_t* ini;

	Ini(const char* data, void* memctx = nullptr) : ini(ini_load(data, memctx))
	{

	}

	~Ini()
	{
		ini_destroy(ini);
	}

	[[nodiscard]] iterator begin() const
	{
		return { ini, 0 };
	}

	[[nodiscard]] iterator end() const
	{
		return { ini, ini_section_count(ini) };
	};

	[[nodiscard]] size_t size() const
	{
		return ini_section_count(ini);
	}

	[[nodiscard]] IniSection operator[](int index) const
	{
		return { ini, index };
	}

	[[nodiscard]] IniSection operator[](const char* section) const
	{
		return { ini, ini_find_section(ini, section, 0) };
	}
};