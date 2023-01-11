#pragma once

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define MODLOADER_IMPLEMENTATION

#include <Windows.h>
#include <detours.h>

#include <cstdint>
#include <cstdio>

#include "ini.h"
#include "Helpers.h"
#include "ModLoader.h"