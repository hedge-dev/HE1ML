#pragma once
class ModLoader;
class FileBinder;
class VirtualFileSystem;
struct CriFunctionTable;
struct Game;

inline const Game* g_game = nullptr;
inline ModLoader* g_loader = nullptr;
inline FileBinder* g_binder = nullptr;
inline VirtualFileSystem* g_vfs = nullptr;
inline CriFunctionTable* g_cri = nullptr;