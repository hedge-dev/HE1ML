struct CriAtomAwbTag
{
    INSERT_PADDING(0x8);
    char* path;
};

static FUNCTION_PTR(void*, __cdecl, criAtomAwbTocGetBinderHandle, 0xC98D83, CriAtomAwbTag* awb, uint32_t* id);
static FUNCTION_PTR(void, __cdecl, criAtomAwbGetWaveFileInfo, 0x9A06D2, CriAtomAwbTag* awb, int32_t id, uint64_t* offset, uint32_t* size);

struct CriAtomPlayerTag
{
    INSERT_PADDING(0x124);
    uint32_t offset;
    uint32_t size;
};

static void criAtomPlayerSetFileCore(CriAtomPlayerTag* player, void* binder, const char* path, uint32_t offset_, uint32_t size_)
{
    static uint32_t funcAddr = 0x994520;
    __asm
    {
        push size_
        push path
        push binder
        mov ecx, player
        mov eax, offset_
        call[funcAddr]
        add esp, 0xC
    }
}

static BOOL __stdcall criAtomPlayerSetWaveIdFileHook(CriAtomPlayerTag* player, CriAtomAwbTag* awb, int32_t id)
{
    void* binder = criAtomAwbTocGetBinderHandle(awb, nullptr);

    char dirPath[0x100];
    strcpy_s(dirPath, awb->path);
    char* suffix = rstrstr(dirPath, "_streamfiles.awb");
    if (suffix != nullptr)
        *suffix = '\0';

    char filePath[0x100];
    sprintf_s(filePath, "%s/%d.adx", dirPath, id);

    std::string resolvedFilePath;
    if (g_binder->ResolvePath(filePath, &resolvedFilePath) == eBindError_None)
    {
        HANDLE hFile = CreateFileA(resolvedFilePath.c_str(), 
            GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);

        if (hFile != nullptr)
        {
            uint32_t fileSize = GetFileSize(hFile, nullptr);
            CloseHandle(hFile);

            criAtomPlayerSetFileCore(player, binder, filePath, 0, fileSize);
            player->offset = 0;
            player->size = fileSize;

            return true;
        }
    }

    uint64_t offset;
    uint32_t size;
    criAtomAwbGetWaveFileInfo(awb, id, &offset, &size);
    criAtomPlayerSetFileCore(player, binder, awb->path, static_cast<uint32_t>(offset), size);
    player->offset = static_cast<uint32_t>(offset);
    player->size = size;

    return true;
}

static void __declspec(naked) criAtomPlayerSetWaveIdFileTrampoline()
{
    __asm
    {
        push[ebp + 8]
        push esi
        push edi
        call criAtomPlayerSetWaveIdFileHook
        retn
    }
}

namespace lw
{
    void InitAwbRedirector()
    {
        WRITE_JUMP(0x9945B5, criAtomPlayerSetWaveIdFileTrampoline);
    }
}