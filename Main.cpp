// ---------------------------------------------------------------------------------------------------------------------
// Digital Sound Keeper v1.2.2 [2022/05/15]
// Prevents SPDIF/HDMI digital audio playback devices from sleeping. Uses WASAPI, requires Windows 7+.
// (C) 2014-2022 Evgeny Vrublevsky <me@veg.by>
// ---------------------------------------------------------------------------------------------------------------------

#include "Common.hpp"
#include "CSoundKeeper.hpp"

void ParseStreamArgs(CSoundKeeper* keeper, KeepStreamType stream_type, char* args)
{
	keeper->SetStreamType(stream_type);
	keeper->SetFrequency(1.00);
	keeper->SetAmplitude(0.01);
	keeper->SetFading(0.1);

	char* p = args;
	while (*p)
	{
		if (*p == ' ' || *p == '-') { p++; }
		else if (*p == 'f' || *p == 'a' || *p == 'l' || *p == 'w' || *p == 't')
		{
			char type = *p;
			p++;
			while (*p == ' ' || *p == '=') { p++; }
			double value = fabs(strtod(p, &p));
			if (type == 'f')
			{
				keeper->SetFrequency(std::min(value, 96000.0));
			}
			else if (type == 'a')
			{
				keeper->SetAmplitude(std::min(value / 100.0, 1.0));
			}
			else if (type == 'l')
			{
				keeper->SetPeriodicPlaying(value);
			}
			else if (type == 'w')
			{
				keeper->SetPeriodicWaiting(value);
			}
			else if (type == 't')
			{
				keeper->SetFading(value);
			}
		}
		else
		{
			break;
		}
	}
}

void ParseMode(CSoundKeeper* keeper, const char* args)
{
	char buf[MAX_PATH];
	strcpy_s(buf, args);
	_strlwr(buf);

	if (strstr(buf, "all"))     { keeper->SetDeviceType(KeepDeviceType::All); }
	if (strstr(buf, "analog"))  { keeper->SetDeviceType(KeepDeviceType::Analog); }
	if (strstr(buf, "digital")) { keeper->SetDeviceType(KeepDeviceType::Digital); }
	if (strstr(buf, "kill"))    { keeper->SetDeviceType(KeepDeviceType::None); }

	if (strstr(buf, "zero") || strstr(buf, "null"))
	{
		keeper->SetStreamType(KeepStreamType::Zero);
	}
	else if (char* p = strstr(buf, "sine"))
	{
		ParseStreamArgs(keeper, KeepStreamType::Sine, p+4);
	}
	else if (char* p = strstr(buf, "noise"))
	{
		ParseStreamArgs(keeper, KeepStreamType::WhiteNoise, p+5);
	}
}

__forceinline int Main()
{
	DebugLog("Main thread started.");

	if (HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE); FAILED(hr))
	{
#ifndef _CONSOLE
		MessageBoxA(0, "Cannot initialize COM.", "Sound Keeper", MB_ICONERROR | MB_OK | MB_SYSTEMMODAL);
#else
		DebugLogError("Cannot initialize COM: 0x%08X.", hr);
#endif
		return hr;
	}

	CSoundKeeper* keeper = new CSoundKeeper();
	keeper->SetDeviceType(KeepDeviceType::Primary);
	keeper->SetStreamType(KeepStreamType::Fluctuate);

	// Parse file name for defaults.
	char fn_buffer[MAX_PATH];
	DWORD fn_size = GetModuleFileNameA(NULL, fn_buffer, MAX_PATH);
	if (fn_size != 0 && fn_size != MAX_PATH)
	{
		char* filename = strrchr(fn_buffer, '\\');
		if (filename)
		{
			filename++;
			DebugLog("Exe File Name: %s.", filename);
			ParseMode(keeper, filename);
		}
	}

	// Parse command line for arguments.
	if (const char* cmdln = GetCommandLineA())
	{
		// Skip program file name.
		while (*cmdln == ' ') { cmdln++; }
		if (cmdln[0] == '"')
		{
			cmdln++;
			while (*cmdln != '"' && *cmdln != 0) { cmdln++; }
			if (*cmdln == '"') { cmdln++; }
		}
		else
		{
			while (*cmdln != ' ' && *cmdln != 0) { cmdln++; }
		}
		while (*cmdln == ' ') { cmdln++; }

		if (*cmdln != 0)
		{
			DebugLog("Command Line: %s.", cmdln);
			ParseMode(keeper, cmdln);
		}
	}

#ifdef _CONSOLE

	switch (keeper->GetDeviceType())
	{
		case KeepDeviceType::None:      DebugLog("Device Type: None."); break;
		case KeepDeviceType::Primary:   DebugLog("Device Type: Primary."); break;
		case KeepDeviceType::All:       DebugLog("Device Type: All."); break;
		case KeepDeviceType::Analog:    DebugLog("Device Type: Analog."); break;
		case KeepDeviceType::Digital:   DebugLog("Device Type: Digital."); break;
		default:                        DebugLogError("Unknown Device Type."); break;
	}

	switch (keeper->GetStreamType())
	{
		case KeepStreamType::Zero:      DebugLog("Stream Type: Zero."); break;
		case KeepStreamType::Fluctuate: DebugLog("Stream Type: Fluctuate."); break;
		case KeepStreamType::Sine:      DebugLog("Stream Type: Sine (Frequency: %.3fHz; Amplitude: %.3f%%; Fading: %.3fs).", keeper->GetFrequency(), keeper->GetAmplitude() * 100.0, keeper->GetFading()); break;
		case KeepStreamType::WhiteNoise:DebugLog("Stream Type: White Noise (Amplitude: %.3f%%; Fading: %.3fs).", keeper->GetAmplitude() * 100.0, keeper->GetFading()); break;
		default:                        DebugLogError("Unknown Stream Type."); break;
	}

	if (keeper->GetPeriodicPlaying() || keeper->GetPeriodicWaiting())
	{
		DebugLog("Periodicity: play %.3fs, wait %.3fs.", keeper->GetPeriodicPlaying(), keeper->GetPeriodicWaiting());
	}

#endif

	HRESULT hr = keeper->Main();
	SafeRelease(keeper);

	CoUninitialize();

#ifndef _CONSOLE
	if (FAILED(hr))
	{
		MessageBoxA(0, "Cannot initialize WASAPI.", "Sound Keeper", MB_ICONERROR | MB_OK | MB_SYSTEMMODAL);
	}
#else
	if (hr == S_OK)
	{
		DebugLog("Main thread finished. Exit code: 0.", hr);
	}
	else
	{
		DebugLog("Main thread finished. Exit code: 0x%08X.", hr);
	}
#endif

	return hr;
}

#ifdef _CONSOLE

int main()
{
	return Main();
}

#else

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow)
{
	return Main();
}

#endif
