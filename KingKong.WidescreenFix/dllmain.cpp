#include "..\includes\stdafx.h"
#include "..\includes\hooking\Hooking.Patterns.h"
#include <string>

HWND hWnd;
bool bDelay;
bool bSettingsApp;
uintptr_t pBarsPtr;
uintptr_t pBarsJmp;

struct Screen
{
	int32_t Width;
	int32_t Height;
	float fWidth;
	float fHeight;
	float fFieldOfView;
	float fAspectRatio;
	float fCustomAspectRatioHor;
	float fCustomAspectRatioVer;
	int32_t Width43;
	float fWidth43;
	float fHudScale;
	float fHudOffset;
	float fCutOffArea;
	float fFMVScale;
} Screen;

void __declspec(naked) BarsHook()
{
	__asm pushad
	if (*(uint32_t*)((*(uintptr_t*)pBarsPtr) + 0xCC) == 3)
	{
		__asm popad
		__asm jmp pBarsJmp
	}
	else
	{
		__asm popad
		__asm retn 0x0C
	}
}

DWORD WINAPI InitSettings(LPVOID)
{
	auto pattern = hook::pattern("75 66 8D 4C 24 04 51");
	if (pattern.size() > 0)
	{
		bSettingsApp = true;
		pattern = hook::pattern("75 66 8D 4C 24 04 51");
		injector::MakeNOP(pattern.get(0).get<uintptr_t>(0), 2, true); //0x40BD3F

		pattern = hook::pattern("75 39 83 7C 24 08 01 75 32 8B"); //0x40BD6C
		struct RegHook
		{
			void operator()(injector::reg_pack& regs)
			{
				regs.ecx = *(uint32_t*)(regs.esp + 0x118);
				regs.edx = (regs.esp + 0x0C);

				GetModuleFileName(NULL, (char*)regs.edx, MAX_PATH);
				*(strrchr((char*)regs.edx, '\\') + 1) = '\0';
			}
		}; injector::MakeInline<RegHook>(pattern.get(0).get<uintptr_t>(0), pattern.get(0).get<uintptr_t>(20));
	}
	return 0;
}

DWORD WINAPI Init(LPVOID)
{
	if (bSettingsApp)
		return 0;

	auto pattern = hook::pattern("33 DB 89 5D E0 53");
	if (!(pattern.size() > 0) && !bDelay)
	{
		bDelay = true;
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)&Init, NULL, 0, NULL);
		return 0;
	}

	if (bDelay)
	{
		while (!(pattern.size() > 0))
			pattern = hook::pattern("33 DB 89 5D E0 53");
	}

	CIniReader iniReader("");
	static bool bCustomAR;
	char *szForceAspectRatio = iniReader.ReadString("MAIN", "ForceAspectRatio", "auto");
	static bool bFullscreenFMVs = iniReader.ReadInteger("MAIN", "FullscreenFMVs", 1) != 0;
	static float fMouseSensitivityFactor = iniReader.ReadFloat("MAIN", "MouseSensitivityFactor", 0.0f);
	static float fFOVFactor = iniReader.ReadFloat("MAIN", "FOVFactor", 0.0f);

	if (strncmp(szForceAspectRatio, "auto", 4) != 0)
	{
		Screen.fCustomAspectRatioHor = static_cast<float>(std::stoi(szForceAspectRatio));
		Screen.fCustomAspectRatioVer = static_cast<float>(std::stoi(strchr(szForceAspectRatio, ':') + 1));
		bCustomAR = true;
	}

	pattern = hook::pattern("89 86 BC 02 00 00 89 8E C0 02 00 00 89 96 C4 02 00 00"); //0x9F2161 
	struct ResHook
	{
		void operator()(injector::reg_pack& regs)
		{
			Screen.Width = regs.eax;
			Screen.Height = regs.ecx;

			*(uint32_t*)(regs.esi + 0x2BC) = Screen.Width;
			*(uint32_t*)(regs.esi + 0x2C0) = Screen.Height;
			*(uint32_t*)(regs.esi + 0x2C4) = regs.edx;

			Screen.fWidth = static_cast<float>(Screen.Width);
			Screen.fHeight = static_cast<float>(Screen.Height);
			Screen.fAspectRatio = Screen.fWidth / Screen.fHeight;
			Screen.Width43 = static_cast<uint32_t>(Screen.fHeight * (4.0f / 3.0f));
			Screen.fWidth43 = static_cast<float>(Screen.Width43);
			Screen.fHudOffset = (1.0f / (Screen.fHeight * (4.0f / 3.0f))) * ((Screen.fWidth - Screen.fHeight * (4.0f / 3.0f)) / 2.0f);
			Screen.fHudScale = Screen.fHeight / Screen.fWidth;
			Screen.fFieldOfView = 1.0f * (((4.0f / 3.0f)) / (Screen.fAspectRatio));
			Screen.fCutOffArea = 0.5f / Screen.fFieldOfView;
			Screen.fFMVScale = 1.0f / (((4.0f / 3.0f)) / (Screen.fAspectRatio));

			if (bCustomAR)
			{
				Screen.fAspectRatio = Screen.fCustomAspectRatioHor / Screen.fCustomAspectRatioVer;
				Screen.fHudScale = Screen.fCustomAspectRatioVer / Screen.fCustomAspectRatioHor;
				Screen.fFieldOfView = 1.0f * (((4.0f / 3.0f)) / (Screen.fAspectRatio));
				Screen.fCutOffArea = 0.5f / Screen.fFieldOfView;
			}

			if (fFOVFactor)
			{
				Screen.fFieldOfView /= fFOVFactor;
				Screen.fCutOffArea = 0.5f / Screen.fFieldOfView;
			}
		}
	}; injector::MakeInline<ResHook>(pattern.get(0).get<uintptr_t>(0), pattern.get(0).get<uintptr_t>(18));

	char pattern_str[20];
	union {
		uint32_t* Int;
		unsigned char Byte[4];
	} dword_temp;

	dword_temp.Int = *hook::pattern("D9 04 8D ? ? ? ? D9 5C 24 18 EB 0A").get(0).get<uint32_t*>(3);
	sprintf(pattern_str, "%02X %02X %c %02X %02X %02X %02X", 0xD9, 0x04, '?', dword_temp.Byte[0], dword_temp.Byte[1], dword_temp.Byte[2], dword_temp.Byte[3]);
	pattern = hook::pattern(pattern_str); //0xA01C67

	for (size_t i = 0; i < pattern.size(); i++)
	{
		injector::MakeNOP(pattern.get(i).get<uint32_t>(0), 1, true);
		injector::WriteMemory<uint16_t>(pattern.get(i).get<uint32_t>(1), 0x05D9, true);
		injector::WriteMemory(pattern.get(i).get<uint32_t>(3), &Screen.fHudScale, true);
	}

	auto ptr = hook::get_pattern("D8 3D ? ? ? ? D9 5C 24 34 E8 ? ? ? ? D9", 2); //0xA01E6A
	injector::WriteMemory(ptr, &Screen.fFieldOfView, true);

	//objects disappear at screen edges with hor+, fixing that @0098B8CF
	ptr = hook::get_pattern("D8 0D ? ? ? ? D9 5C 24 10 8B 5C 24 10 53", 2); //0x98B8CF
	injector::WriteMemory(ptr, &Screen.fCutOffArea, true);

	//FMVs
	pattern = hook::pattern("89 50 18 8B 06 8B CE FF 50 14 8B 4F 08"); //0xA25984
	struct FMVHook
	{
		void operator()(injector::reg_pack& regs)
		{ 
			//hor
			*(float*)(regs.eax - 0x54) /= Screen.fFMVScale;
			*(float*)(regs.eax - 0x38) /= Screen.fFMVScale;

			*(float*)(regs.eax - 0x1C) /= Screen.fFMVScale;
			*(float*)(regs.eax + 0x00) /= Screen.fFMVScale;

			if (bFullscreenFMVs)
			{
				static const float fFMVSize = (640.0f / 386.0f) / (480.0f / 386.0f);
				//hor
				*(float*)(regs.eax - 0x54) *= fFMVSize;
				*(float*)(regs.eax - 0x38) *= fFMVSize;

				*(float*)(regs.eax - 0x1C) *= fFMVSize;
				*(float*)(regs.eax + 0x00) *= fFMVSize;

				//ver
				*(float*)(regs.eax - 0x50) *= fFMVSize;
				*(float*)(regs.eax - 0x18) *= fFMVSize;

				*(float*)(regs.eax - 0x34) *= fFMVSize;
				*(float*)(regs.eax + 0x04) *= fFMVSize;
			}
			
			*(uintptr_t*)(regs.eax + 0x18) = regs.edx;
			regs.eax = *(uintptr_t*)(regs.esi);
		}
	}; injector::MakeInline<FMVHook>(pattern.get(0).get<uintptr_t>(0));

	pattern = hook::pattern("68 ? ? ? ? E8 ? ? ? ? 50 55 56 E8 ? ? ? ? 0F B7 4F 06 0F B7 57 04"); //0x992C4C
	static auto unk_CC0E20 = *pattern.get(0).get<char*>(1);
	struct TextHook
	{
		void operator()(injector::reg_pack& regs)
		{
			if (strncmp(unk_CC0E20, "16/9", 4) == 0)
			{
				strncpy(unk_CC0E20, "Bars", 4);
			}
			else
			{
				if (strncmp(unk_CC0E20, "4/3", 3) == 0)
				{
					strncpy(unk_CC0E20, "Std", 4);
				}
			}

			regs.ecx = *(uint16_t*)(regs.edi + 6);
			regs.edx = *(uint16_t*)(regs.edi + 4);
		}
	}; injector::MakeInline<TextHook>(pattern.get(0).get<uintptr_t>(18), pattern.get(0).get<uintptr_t>(18 + 8));

	pBarsPtr = *(uintptr_t*)hook::get_pattern("A1 ? ? ? ? 8B 88 00 08 00 00 81 C1 B4 68 06 00", 1); //0xCBFBE0
	auto off_AD5CE8 = *hook::pattern("89 46 1C C7 06 ? ? ? ? 8B C6 5E C3").get(1).get<uintptr_t*>(5);
	injector::WriteMemory(off_AD5CE8 + 5, BarsHook, true); //0xAD5CFC
	pBarsJmp = (uintptr_t)hook::get_pattern("8B 41 10 6A 00 50 B9 ? ? ? ? E8", 0); //0xA2A350

	if (fMouseSensitivityFactor)
	{
		pattern = hook::pattern("D9 85 ? ? ? ? D8 1D ? ? ? ? DF E0 F6 C4 41 75 1D D9 85 4C"); //0x45F048
		struct MouseSensHook
		{
			void operator()(injector::reg_pack& regs)
			{
				*(float*)(regs.ebp - 0x1B0) *= fMouseSensitivityFactor;
				*(float*)(regs.ebp - 0x1B4) *= fMouseSensitivityFactor;

				float temp = *(float*)(regs.ebp - 0x1B4);
				_asm fld     dword ptr[temp]
			}
		}; injector::MakeInline<MouseSensHook>(pattern.get(0).get<uintptr_t>(0), pattern.get(0).get<uintptr_t>(6));
	}
	return 0;
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		InitSettings(NULL);
		Init(NULL);
	}
	return TRUE;
}