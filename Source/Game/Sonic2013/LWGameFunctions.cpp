#include <CRIWARE/Criware.h>

CriErrCbFunc& g_err_callback = *reinterpret_cast<CriErrCbFunc*>(0x00FF8D54);
void CRIAPI criErr_SetCallback_LW(CriErrCbFunc cbf)
{
	g_err_callback = cbf;
}