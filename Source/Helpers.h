﻿#pragma once

#define _CONCAT2(x, y) x##y
#define CONCAT2(x, y) _CONCAT(x, y)
#define INSERT_PADDING(length) \
    uint8_t CONCAT2(pad, __LINE__)[length]

#define ASSERT_OFFSETOF(type, field, offset) \
    static_assert(offsetof(type, field) == offset, "offsetof assertion failed")

#define ASSERT_SIZEOF(type, size) \
    static_assert(sizeof(type) == size, "sizeof assertion failed")

#ifdef BASE_ADDRESS
const HMODULE MODULE_HANDLE = GetModuleHandle(nullptr);

#define ASLR(address) \
    ((size_t)MODULE_HANDLE + (size_t)address - (size_t)BASE_ADDRESS)
#endif

#define FUNCTION_PTR(returnType, callingConvention, function, location, ...) \
    returnType (callingConvention *function)(__VA_ARGS__) = (returnType(callingConvention*)(__VA_ARGS__))(location)

#define PROC_ADDRESS(libraryName, procName) \
    GetProcAddress(LoadLibrary(TEXT(libraryName)), procName)

#define HOOK(returnType, callingConvention, functionName, location, ...) \
    typedef returnType callingConvention functionName(__VA_ARGS__); \
    functionName* original##functionName = (functionName*)(location); \
    returnType callingConvention implOf##functionName(__VA_ARGS__)

#define INSTALL_HOOK_ADDRESS(functionName, ADDRESS) \
    { \
		*(void**)&original##functionName = (void*)ADDRESS; \
        DetourTransactionBegin(); \
        DetourUpdateThread(GetCurrentThread()); \
        DetourAttach((void**)&original##functionName, implOf##functionName); \
        DetourTransactionCommit(); \
    }

#define INSTALL_HOOK(functionName) INSTALL_HOOK_ADDRESS(functionName, original##functionName)

#define VTABLE_HOOK(returnType, callingConvention, className, functionName, functionIndex, ...) \
    typedef returnType callingConvention className##functionName(className* This, __VA_ARGS__); \
    className##functionName* original##className##functionName; \
    unsigned className##functionName##Index = functionIndex; \
    returnType callingConvention implOf##className##functionName(className* This, __VA_ARGS__)

#define INSTALL_VTABLE_HOOK(className, object, functionName) \
    do { \
        if (original##className##functionName == nullptr) \
        { \
            original##className##functionName = (*(className##functionName***)object)[className##functionName##Index]; \
            DetourTransactionBegin(); \
            DetourUpdateThread(GetCurrentThread()); \
            DetourAttach((void**)&original##className##functionName, implOf##className##functionName); \
            DetourTransactionCommit(); \
        } \
    } while(0)

#define WRITE_MEMORY(location, type, ...) \
    { \
        const type data[] = { __VA_ARGS__ }; \
        DWORD oldProtect; \
        VirtualProtect((void*)(location), sizeof(data), PAGE_EXECUTE_READWRITE, &oldProtect); \
        memcpy((void*)(location), data, sizeof(data)); \
        VirtualProtect((void*)(location), sizeof(data), oldProtect, &oldProtect); \
    }

#define WRITE_JUMP(location, function) \
    { \
        WRITE_MEMORY(location, uint8_t, 0xE9); \
        WRITE_MEMORY(location + 1, uint32_t, (uint32_t)(function) - (size_t)(location) - 5); \
    }
	
#define WRITE_CALL(location, function) \
    { \
        WRITE_MEMORY(location, uint8_t, 0xE8); \
        WRITE_MEMORY(location + 1, uint32_t, (uint32_t)(function) - (size_t)(location) - 5); \
    }

#define WRITE_NOP(location, count) \
    { \
        DWORD oldProtect; \
        VirtualProtect((void*)(location), (size_t)(count), PAGE_EXECUTE_READWRITE, &oldProtect); \
        for (size_t i = 0; i < (size_t)(count); i++) \
            *((uint8_t*)(location) + i) = 0x90; \
        VirtualProtect((void*)(location), (size_t)(count), oldProtect, &oldProtect); \
    }

template<int N>
struct padding_t
{
    INSERT_PADDING(N);
};