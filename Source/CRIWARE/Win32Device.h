#pragma once
struct CriFsIoInterfaceTag;
class FileBinder;

void CriFsIoWin_SetBinder(FileBinder* binder);
extern CriFsIoInterfaceTag win_io_interface;