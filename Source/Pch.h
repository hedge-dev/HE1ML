#pragma once

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define MODLOADER_IMPLEMENTATION

#define BASE_ADDRESS 0x400000
#include <Windows.h>
#include <detours.h>

#include <cstdint>
#include <cstdio>
#include <unordered_map>

#include "ini.hpp"
#include "Helpers.h"
#include "ModLoader.h"