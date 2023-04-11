#pragma once

namespace bb
{
	bool GetValue(size_t key, void** value);
	bool EventProc(size_t key, void* value);
}