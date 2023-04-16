#pragma once

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define MODLOADER_IMPLEMENTATION

#define BASE_ADDRESS 0x400000
#define NOMINMAX

#include <Windows.h>
#include <detours.h>

#include <cstdint>
#include <cstdio>
#include <unordered_map>

#include <CommonLoader.h>

#include "Globals.h"
#include "Helpers.h"
#include "ini.hpp"
#include "ModLoader.h"
#include "StubFunctions.h"
#include "Utilities.h"
