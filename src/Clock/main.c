//===============================================================================
//--+++--> main.c - KAZUBON 1997-2001 ============================================
//--+++--> WinMain, window procedure, and functions for initializing ==============
//==================== Last Modified by Stoic Joker: Sunday, 03/13/2011 @ 11:54:05am
#include "tclock.h" //---------------{ Stoic Joker 2006-2011 }---------------+++-->
#include <winver.h>
#include <wtsapi32.h>
#include <shlobj.h>//SHGetFolderPath
#include <time.h>
#ifdef _MSC_VER
#	include <direct.h>
#	define chdir _chdir
#else
#	include <unistd.h> // chdir
#endif
#include "../common/version.h"

HINSTANCE g_instance;
TClockAPI api;

// Application Global Window Handles
HWND	g_hwndTClockMain = NULL;
HWND	g_hwndClock = NULL;
HWND	g_hwndSheet = NULL;
HWND	g_hDlgTimer = NULL;
HWND	g_hDlgStopWatch = NULL;
HWND	g_hDlgTimerWatch = NULL;
HWND	g_hDlgSNTP = NULL;

HICON	g_hIconTClock, g_hIconPlay, g_hIconStop, g_hIconDel;

// used by PageMisc.c and main.c
const wchar_t kSectionImmersiveShell[56+1] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ImmersiveShell";
const wchar_t kKeyWin32Tray[27+1] = L"UseWin32TrayClockExperience";

LRESULT CALLBACK Window_TClock(HWND, UINT, WPARAM, LPARAM);

ATOM g_atomTClock = 0; /**< main window class atom */
const wchar_t g_szClassName[] = L"TClockMainClass"; /**< our main window class name */
UINT g_WM_TaskbarCreated = 0; /**< TaskbarCreated message (broadcasted on Explorer (re-)start) */

/** \brief processes our commandline parameters
 * \param hwndMain clock hwnd of master instance
 * \param cmdline commandline parameters
 * \remarks if hwndMain doesn't match \c g_hwndTClockMain, it'll create a message-only window for sound processing
 * \sa g_hwndTClockMain, WinMain() */
static void ProcessCommandLine(HWND hwndMain,const wchar_t* cmdline);
static void OnTimerMain(HWND hwnd);
//static void FindTrayServer(); // Redux: what ever it was supposed to be..
static void InitError(int n);

// alarm.c
extern char g_bPlayingNonstop;


//=================================================================
//---------------------------+++--> fixes lost-keyboard-control bug:
BOOL EnableDlgItemSafeFocus(HWND hDlg,int control,BOOL bEnable,int nextFocus)
{
	HWND hwnd=GetDlgItem(hDlg,control);
	if(!bEnable && GetFocus()==hwnd){
		if(nextFocus){
			HWND hwndnext=GetDlgItem(hDlg,nextFocus);
			SendMessage(hDlg,WM_NEXTDLGCTL,(WPARAM)hwndnext,TRUE);
		}else
			SendMessage(hDlg,WM_NEXTDLGCTL,0,FALSE);
	}
	return EnableWindow(hwnd,bEnable);
}

static void CALLBACK ToggleCalendar_done(HWND hwnd, UINT uMsg, ULONG_PTR dwData, LRESULT lResult){
	(void)uMsg;
	(void)lResult;
	if(api.OS >= TOS_WIN10_1)
		return;
	hwnd = api.GetCalendar();
	if(!hwnd && api.OS >= TOS_WIN10){ // Win10 (new slow calendar)
		dwData = 50;
		do{ // min 6-12 iterations on my VM (also seen 50+ under load)
			Sleep(50);
			hwnd = api.GetCalendar();
		}while(!hwnd && --dwData);
	}
	// 11px padding is Win8 default, 0px is Win10
	if(hwnd)
		api.PositionWindow(hwnd, (api.OS<TOS_WIN10 && api.OS>=TOS_WIN7 ? 11 : 0));
}
//=================================================================
//--------------------------+++--> toggle calendar (close or open):
void ToggleCalendar(int type)   //---------------------------+++-->
{
	HWND calendar = api.GetCalendar();
	int is_custom = api.GetInt(L"Calendar", L"bCustom", 0);
	if(calendar){
		if(is_custom)
			SetForegroundWindow(calendar);
		return;
	}
	if(api.OS >= TOS_VISTA && (!is_custom && type!=1)){
		// Windows 10 workaround as SendMessage doesn't work any longer (no error given)
		SendMessageCallback(g_hwndClock, WM_USER+102, 1, 0, ToggleCalendar_done, 0);//1=open, 0=close
	}else{
		wchar_t cal[MAX_PATH];
		memcpy(cal, api.root, api.root_size);
		add_title(cal,L"misc\\XPCalendar.exe");
		api.Exec(cal,NULL,g_hwndTClockMain);
	}
}

static BOOL m_bMonOffOnLock = FALSE;
//================================================================================================
//------------------------------+++--> UnRegister the Clock For Login Session Change Notifications:
void UnregisterSession(HWND hwnd)   //--------{ Explicitly Linked for Windows 2000 }--------+++-->
{
	if(!m_bMonOffOnLock)
		return;
	m_bMonOffOnLock = !WTSUnRegisterSessionNotification(hwnd);
}
//================================================================================================
//--------------------------------+++--> Register the Clock For Login Session Change Notifications:
void RegisterSession(HWND hwnd)   //---------{ Explicitly Linked for Windows 2000 }---------+++-->
{
	if(m_bMonOffOnLock)
		return;
	m_bMonOffOnLock = WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_ALL_SESSIONS);
}
//====================================================================================
//---------------------------+++--> Does our startup file exist? Also creates filename:
int GetStartupFile(HWND hDlg, wchar_t filename[MAX_PATH]){   //--------------------+++-->
	size_t offset;
	if(SHGetFolderPath(hDlg,CSIDL_STARTUP,NULL,SHGFP_TYPE_CURRENT,filename)!=S_OK){
		return 0;
	}
	offset = wcslen(filename);
	filename[offset] = '\\';
	filename[offset+1] = '\0'; // old Stoic Joker link
	wcscat(filename, CONF_START_OLD);
	wcscat(filename, L".lnk");
	if(PathFileExists(filename))
		return 1;
	filename[offset+1] = '\0'; // new name
	wcscat(filename, CONF_START);
	wcscat(filename, L".lnk");
	if(PathFileExists(filename))
		return 1;
	return 0;
}
//================================================================================================
//----------------------------------------+++--> Remove Launch T-Clock on Windows Startup ShortCut:
void RemoveStartup(HWND hDlg)   //----------------------------------------------------------+++-->
{
	wchar_t path[MAX_PATH];
	if(!GetStartupFile(hDlg,path))
		return;
	DeleteFile(path);
}
//===================================
void AddStartup(HWND hDlg) //--+++-->
{
	wchar_t path[MAX_PATH], myexe[MAX_PATH];
	if(GetStartupFile(hDlg,path) || !*path)
		return;
	*wcsrchr(path, '\\') = '\0';
	GetModuleFileName(g_instance, myexe, _countof(path));
	CreateLink(myexe, path, CONF_START);
}
//==========================
//--+++--> Create Launch T-Clock on Windows Startup ShortCut:
int CreateLink(wchar_t* fname, wchar_t* dstpath, wchar_t* name)
{
	HRESULT hres;
	IShellLink* psl;
	
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	
	hres = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLink, (void**)&psl);
	if(SUCCEEDED(hres)) {
		IPersistFile* ppf;
		
		psl->lpVtbl->SetPath(psl, fname);
		psl->lpVtbl->SetDescription(psl, name);
		
		hres = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void**)&ppf);
		if(SUCCEEDED(hres)) {
			wchar_t lnkfile[MAX_PATH];
			wcsncpy_s(lnkfile, MAX_PATH, dstpath, _TRUNCATE);
			add_title(lnkfile, name);
			wcscat(lnkfile, L".lnk");
			
			hres = ppf->lpVtbl->Save(ppf, lnkfile, 1);
			ppf->lpVtbl->Release(ppf);
		}
		psl->lpVtbl->Release(psl);
	}
	CoUninitialize();
	
	return SUCCEEDED(hres);
}

void TranslateDispatchTClockMessage(MSG* msg) {
	if(!(g_hwndSheet && IsWindow(g_hwndSheet) && PropSheet_IsDialogMessage(g_hwndSheet,msg))
	&& !(g_hDlgTimer && IsWindow(g_hDlgTimer) && IsDialogMessage(g_hDlgTimer,msg))
	&& !(g_hDlgTimerWatch && IsWindow(g_hDlgTimerWatch) && IsDialogMessage(g_hDlgTimerWatch,msg))
	&& !(g_hDlgSNTP && IsWindow(g_hDlgSNTP) && IsDialogMessage(g_hDlgSNTP,msg))
	&& !(g_hDlgStopWatch && IsWindow(g_hDlgStopWatch) && IsDialogStopWatchMessage(g_hDlgStopWatch,msg))){
		TranslateMessage(msg);
		DispatchMessage(msg);
	}
}

//================================================================================================
//--------------------------------------------------==-+++--> Entry Point of Program Using WinMain:
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, wchar_t* lpCmdLine, int nShowCmd)
{
	WNDCLASS wndclass;
	HWND hwndMain;
	MSG msg;
	int updated;
	
	(void)hPrevInstance;
	(void)nShowCmd;
	
	g_instance = hInstance;
	updated = LoadClockAPI(L"misc/T-Clock" ARCH_SUFFIX, &api);
	if(updated) {
		wchar_t title[16];
		swprintf(title, _countof(title), FMT("API error (%i)"), updated);
		MessageBox(NULL, L"Error loading: T-Clock" ARCH_SUFFIX L".dll", title, (MB_OK | MB_ICONERROR | MB_SETFOREGROUND));
		return 2;
	}
	_wchdir(api.root); // make sure we've got the right working directory
	
	// Make sure we're running Windows 2000 and above
	if(!api.OS) {
		MessageBox(NULL, L"T-Clock requires Windows 2000 or newer", L"old OS", MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
		return 1;
	}
	
	// make sure ObjectBar isn't running -> From Original Code/Unclear if This is Still a Conflict. (test suggested not really.. no crash but no clock either :P)
	if(FindWindowA("ObjectBar Main","ObjectBar")) {
		MessageBox(NULL, L"ObjectBar and T-Clock can't be run together", L"ObjectBar detected!" ,MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
		return 1;
	}
	
	// Load ALL of the Global Resources
	g_hIconTClock = LoadIcon(api.hInstance, MAKEINTRESOURCE(IDI_MAIN));
	g_hIconPlay = LoadImage(g_instance, MAKEINTRESOURCE(IDI_PLAY), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	g_hIconStop = LoadImage(g_instance, MAKEINTRESOURCE(IDI_STOP), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	g_hIconDel  = LoadImage(g_instance, MAKEINTRESOURCE(IDI_DEL), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	
//	FindTrayServer(hwndMain);
	
	// Do Not Allow the Program to Execute Twice!
	updated = 25; /**< wait up to 5 sec in 1/5th seconds for other instance */
	do{
		HANDLE processlock = CreateMutex(NULL, FALSE, g_szClassName); // we leak handle here, but Windows closes on process exit anyway (so why do it manually?)
		if(processlock && GetLastError()==ERROR_ALREADY_EXISTS){
			CloseHandle(processlock);
			hwndMain = FindWindow(g_szClassName, NULL);
			if(hwndMain) { // This One Sends Commands to the Instance
				ProcessCommandLine(hwndMain, lpCmdLine); // That is Currently Running.
				return 0;
			}
			Sleep(200);
			continue;
		}
		// Make sure we're not running 32bit on a 64bit OS / start the other one
		#ifndef _WIN64
		if(IsWow64()){
			wchar_t clock64[MAX_PATH];
			CloseHandle(processlock);
			memcpy(clock64, api.root, api.root_size);
			add_title(clock64, L"Clock" ARCH_SUFFIX_64 L".exe");
			api.ShellExecute(NULL, clock64, lpCmdLine, NULL, SW_SHOWNORMAL, &processlock);
			if(processlock) {
				for(; (lpCmdLine = wcschr(lpCmdLine,'/'))!=NULL; ++lpCmdLine) { // MSVC sucks == true
					if(!wcsncasecmp(lpCmdLine,L"/exit",5)) {
						WaitForSingleObject(processlock, INFINITE);
						break;
					}
				}
				CloseHandle(processlock);
			}
			return 0;
		}
		#endif // _WIN64
		break;
	}while(updated--);
	
	// Update settings if required and setup defaults
	if((updated=CheckSettings())<0){
		return 1;
	}
	
	// Message of the taskbar recreating - Special thanks to Mr.Inuya
	g_WM_TaskbarCreated = RegisterWindowMessageA("TaskbarCreated");
	
	// register a window class
	wndclass.style         = 0;
	wndclass.lpfnWndProc   = Window_TClock;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = 0;
	wndclass.hInstance     = g_instance;
	wndclass.hIcon         = g_hIconTClock;
	wndclass.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)(intptr_t)(COLOR_WINDOW+1);
	wndclass.lpszMenuName  = NULL;
	wndclass.lpszClassName = g_szClassName;
	g_atomTClock = RegisterClass(&wndclass);
	
	if(api.OS >= TOS_VISTA) { // allow non elevated processes to send control messages (eg, App with admin rights, explorer without)
		ChangeWindowMessageFilter_t ChangeWindowMessageFilter = (ChangeWindowMessageFilter_t)GetProcAddress(GetModuleHandleA("user32"), "ChangeWindowMessageFilter");
		if(ChangeWindowMessageFilter){
			int msgid;
			ChangeWindowMessageFilter(g_WM_TaskbarCreated,MSGFLT_ADD);
			ChangeWindowMessageFilter(WM_COMMAND,MSGFLT_ADD);
			for(msgid=WM_MOUSEFIRST; msgid<=WM_MOUSELAST; ++msgid)
				ChangeWindowMessageFilter(msgid,MSGFLT_ADD);
			for(msgid=MAINMFIRST; msgid<=MAINMLAST; ++msgid)
				ChangeWindowMessageFilter(msgid,MSGFLT_ADD);
		}
	}
	
	// create a hidden window
	g_hwndTClockMain = hwndMain = CreateWindowEx(WS_EX_NOACTIVATE, MAKEINTATOM(g_atomTClock),NULL, 0, 0,0,0,0, NULL,NULL,g_instance,NULL);
	// This Checks for First Instance Startup Options
	ProcessCommandLine(hwndMain, lpCmdLine);
	
	RegisterHotkeys(hwndMain, 1);
	
	if(api.OS > TOS_2000) {
		if(api.GetInt(L"Desktop", L"MonOffOnLock", 0))
			RegisterSession(hwndMain);
	}
	if(updated==1){
		PostMessage(hwndMain,WM_COMMAND,IDM_SHOWPROP,0);
	}
	while(GetMessage(&msg, NULL, 0, 0)) {
		TranslateDispatchTClockMessage(&msg);
	}
	
	RegisterHotkeys(hwndMain, 0);
	
	UnregisterSession(hwndMain);
	
	EndNewAPI(NULL);
	
	return (int)msg.wParam;
}

LRESULT CALLBACK Window_TClockDummy(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	(void)wParam; (void)lParam;
	
	switch(message) {
	case MM_MCINOTIFY: // stop playing or repeat mci file (all but .wav, .pcb)
		OnMCINotify(hwnd);
		return 0;
	case MM_WOM_DONE: // stop playing wave
	case MAINM_STOPSOUND:
		StopFile();
		return 0;
	}
	return 0;
}
//========================================================================================
//	/exit		exit T-Clock
//	/prop		show T-Clock Options
//	/SyncOpt	SNTP options
//	/Sync		synchronize the system clock with an NTP server
//	/start		start the Stopwatch (open as needed)
//	/stop		stop (pause really) the Stopwatch
//	/reset		reset Stopwatch to 0 (stop as needed)
//	/lap		record a (the current) lap time
//================================================================================================
//---------------------------------------------//---------------+++--> T-Clock Command Line Option:
void ProcessCommandLine(HWND hwndMain,const wchar_t* cmdline)   //-----------------------------+++-->
{
	int just_elevated = 0;
	const wchar_t* p = cmdline;
	if(!g_hwndTClockMain){
		AllowSetForegroundWindow(ASFW_ANY);
		g_hwndTClockMain = CreateWindow(L"STATIC", NULL, 0, 0,0,0,0, HWND_MESSAGE_nowarn, 0, 0, 0);
		SubclassWindow(g_hwndTClockMain, Window_TClockDummy);
	}
	
	while(*p != '\0') {
		if(*p == '/') {
			++p;
			if(wcsncasecmp(p, L"prop", 4) == 0) {
				SendMessage(hwndMain, WM_COMMAND, IDM_SHOWPROP, 0);
				p += 4;
			} else if(wcsncasecmp(p, L"exit", 4) == 0) {
				SendMessage(hwndMain, MAINM_EXIT, 0, 0);
				p += 4;
			} else if(wcsncasecmp(p, L"start", 5) == 0) {
				SendMessage(hwndMain, WM_COMMAND, IDM_STOPWATCH_START, 0);
				p += 5;
			} else if(wcsncasecmp(p, L"stop", 4) == 0) {
				SendMessage(hwndMain, WM_COMMAND, IDM_STOPWATCH_STOP, 0);
				p += 4;
			} else if(wcsncasecmp(p, L"reset", 5) == 0) {
				SendMessage(hwndMain, WM_COMMAND, IDM_STOPWATCH_RESET, 0);
				p += 5;
			} else if(wcsncasecmp(p, L"pause", 5) == 0) {
				SendMessage(hwndMain, WM_COMMAND, IDM_STOPWATCH_PAUSE, 0);
				p += 5;
			} else if(wcsncasecmp(p, L"resume", 6) == 0) {
				SendMessage(hwndMain, WM_COMMAND, IDM_STOPWATCH_RESUME, 0);
				p += 6;
			} else if(wcsncasecmp(p, L"lap", 3) == 0) {
				SendMessage(hwndMain, WM_COMMAND, IDM_STOPWATCH_LAP, 0);
				p += 3;
			} else if(wcsncasecmp(p, L"SyncOpt", 7) == 0) {
				if(!SendMessage(hwndMain, WM_COMMAND, MAKEWPARAM(IDM_SNTP,just_elevated), 0)) {
					NetTimeConfigDialog(just_elevated);
				}
				p += 7;
			} else if(wcsncasecmp(p, L"Sync", 4) == 0) {
				p += 4;
				if(!SendMessage(hwndMain, WM_COMMAND, MAKEWPARAM(IDM_SNTP_SYNC,just_elevated), 0)) {
					SyncTimeNow();
				}
				SendMessage(g_hwndTClockMain, MAINM_EXIT, 0, 0);
			} else if(wcsncmp(p, L"Wc", 2) == 0) { // Win10 calendar "restore"
				if(p[2] == '1') // restore to previous
					api.SetSystemInt(HKEY_LOCAL_MACHINE, kSectionImmersiveShell, kKeyWin32Tray, 1);
				else // use the slow (new) one
					api.DelSystemValue(HKEY_LOCAL_MACHINE, kSectionImmersiveShell, kKeyWin32Tray);
				p += 2;
			} else if(wcsncmp(p, L"UAC", 3) == 0) {
				just_elevated = 1;
				p += 3;
			} else if(wcsncmp(p, L"SEH", 3) == 0) {
				p += 3;
				SendMessage(api.GetClock(0), WM_COMMAND, IDM_SHUTDOWN, 0);
			}
			continue;
		}
		++p;
	}
	
	if(g_hwndTClockMain != hwndMain){
		const DWORD kTimeout = 10000;
		const DWORD kStartTicks = GetTickCount();
		DWORD timeout;
		MSG msg;
		msg.message = 0;
		for(;;){
			int have_ui = IsWindow(g_hwndSheet) || IsWindow(g_hDlgTimer) || IsWindow(g_hDlgTimerWatch) || IsWindow(g_hDlgSNTP) || IsWindow(g_hDlgStopWatch);
			if(have_ui)
				timeout = INFINITE;
			else if(IsPlaying())
				timeout = 200;
			else
				break;
			MsgWaitForMultipleObjectsEx(0, NULL, timeout, QS_ALLEVENTS, MWMO_INPUTAVAILABLE);
			while(PeekMessage(&msg,NULL,0,0,PM_REMOVE) && msg.message != WM_QUIT) {
				TranslateDispatchTClockMessage(&msg);
			}
			if(msg.message == WM_QUIT)
				break;
			if(!have_ui) {
				DWORD elapsed = GetTickCount() - kStartTicks;
				if(elapsed >= kTimeout)
					break;
			}
		}
		DestroyWindow(g_hwndTClockMain);
		g_hwndTClockMain = NULL;
	}
}
static void InjectClockHook(HWND hwnd) {
	static DWORD s_restart_ticks = 0;
	static int s_restart_num = 0;
	DWORD ticks = GetTickCount();
	if(ticks - s_restart_ticks < 2300){
		if(++s_restart_num >= 3){
			if(api.Message(0,
					L"Multiple Explorer crashes or restarts detected\n"
					L"It's possible that T-Clock is crashing your Explorer,\n"
					L"automated hooking postponed.\n"
					L"\n"
					L"Take precaution and exit T-Clock now?", L"T-Clock", MB_YESNO|MB_SETFOREGROUND, MB_ICONEXCLAMATION) == IDYES) {
				SendMessage(hwnd, WM_CLOSE, 0, 0);
				return;
			}
			s_restart_ticks = GetTickCount();
			s_restart_num = 0;
		}
	}else{
		s_restart_ticks = ticks;
		s_restart_num = 0;
	}
	api.Inject(hwnd);
	#ifndef _DEBUG
	EmptyWorkingSet(GetCurrentProcess());
	#endif
}
//================================================================================================
//--------------------------------------------------+++--> The Main Application "Window" Procedure:
LRESULT CALLBACK Window_TClock(HWND hwnd,	UINT message, WPARAM wParam, LPARAM lParam)   //-+++-->
{
	const unsigned kIdleMS = 5000;
	switch(message) {
	case WM_CREATE:
		InitAlarm();  // initialize alarms
		SetTimer(hwnd, IDTIMER_MAIN, 1000, NULL);
		InjectClockHook(hwnd);
		return 0;
		
	case WM_TIMER:{
		switch(wParam) {
		case IDTIMER_MAIN:
			OnTimerMain(hwnd);
			break;
		case IDTIMER_MOUSE:
			OnTimerMouse(hwnd);
			break;
		case IDTIMER_MONITOR:{
			static unsigned lastidle_ = 0;
			unsigned current = GetTickCount();
			LASTINPUTINFO li;
			li.cbSize = sizeof(li);
			GetLastInputInfo(&li);
			if(li.dwTime != lastidle_) {
				if(li.dwTime > current) // 'current' overflow
					li.dwTime = 0;
				li.dwTime += kIdleMS;
				if(li.dwTime <= current) {
					lastidle_ = (li.dwTime - kIdleMS);
					li.dwTime = kIdleMS;
					SendMessage(HWND_BROADCAST_nowarn, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
				} else {
					li.dwTime -= current;
				}
				SetTimer(hwnd, IDTIMER_MONITOR, li.dwTime, NULL);
			}
			break;}
		}
		return 0;}
	case WM_WTSSESSION_CHANGE:{
		WTS_CONNECTSTATE_CLASS* state;
		DWORD size;
		switch(wParam) {
		case WTS_CONSOLE_DISCONNECT: // "switch user", creates new session
			if(WTSGetActiveConsoleSessionId() != 0xFFFFFFFF)
				break;
			/* fall through */
		case WTS_SESSION_LOCK: // "lock", keeps current session
			SetTimer(hwnd, IDTIMER_MONITOR, kIdleMS, NULL);
			break;
		case WTS_CONSOLE_CONNECT:
		case WTS_SESSION_UNLOCK:
			WTSQuerySessionInformationW(NULL, (DWORD)lParam, WTSConnectState, (wchar_t**)&state, &size);
			if(*state == WTSActive)
				KillTimer(hwnd, IDTIMER_MONITOR);
			WTSFreeMemory(state);
			break;
		}
		return 0;}
		
	case WM_ENDSESSION:
		if(!wParam)
			break;
		/* fall through */
	case WM_DESTROY:
		KillTimer(hwnd, IDTIMER_MAIN);
		if(g_hwndSheet && IsWindow(g_hwndSheet))
			SendMessage(g_hwndSheet, WM_CLOSE, 0, 0);
		if(g_hDlgTimer && IsWindow(g_hDlgTimer))
			SendMessage(g_hDlgTimer, WM_CLOSE, 0, 0);
		if(g_hDlgStopWatch && IsWindow(g_hDlgStopWatch))
			SendMessage(g_hDlgStopWatch, WM_CLOSE, 0, 0);
		g_hwndSheet = g_hDlgTimer = g_hDlgStopWatch = NULL;
		EndAlarm();
		EndAllTimers();
		api.Exit(); // exit clock, remove injection
		if(message!=WM_ENDSESSION)
			PostQuitMessage(0);
		return 0;
	case WM_PAINT: {
//			HDC hdc;
//			PAINTSTRUCT ps;
//			hdc = BeginPaint(hwnd, &ps);
//			EndPaint(hwnd, &ps);
			return 0;}
	case WM_HOTKEY:
		return HotkeyMessage(hwnd, wParam, lParam);
		//==================================================
	case MAINM_CLOCKINIT: // Messages sent/posted from TCDLL.dll
		g_hwndClock = (HWND)lParam;
		api.InjectFinalize(); // injected, now remove hook
		return 0;
		
	case MAINM_ERROR:    // error
		InitError((int)lParam);
		SendMessage(hwnd, WM_CLOSE, 0, 0);
		return 0;
		
	case MAINM_EXIT:    // exit
		SendMessage(hwnd, WM_CLOSE, 0, 0);
		return 0;
		
	case MAINM_BLINKOFF:    // clock no longer blinks
		if(!g_bPlayingNonstop) StopFile();
		return 0;
		
	case MM_MCINOTIFY: // stop playing or repeat mci file (all but .wav, .pcb)
		OnMCINotify(hwnd);
		return 0;
	case MM_WOM_DONE: // stop playing wave
	case MAINM_STOPSOUND:
		StopFile();
		return 0;
		
	case WM_WININICHANGE:
		RefreshUs();
		return 0;
	// inform clock about DWM color change
	case WM_DWMCOLORIZATIONCOLORCHANGED:
		api.On_DWMCOLORIZATIONCOLORCHANGED((unsigned)wParam, (BOOL)lParam);
		PostMessage(g_hwndClock, WM_DWMCOLORIZATIONCOLORCHANGED, wParam, lParam);
		return 0;
		
	// context menu
	case WM_COMMAND:
		return OnTClockCommand(hwnd, wParam); // menu.c
		
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_XBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDBLCLK:
	case WM_XBUTTONDBLCLK:
		if(!g_bPlayingNonstop) PostMessage(hwnd, MAINM_STOPSOUND, 0, 0);
		/* fall through */
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_XBUTTONUP:
		OnMouseMsg(hwnd, message, wParam, lParam); // mouse.c
		return 0;
	
	default:
		if(message == g_WM_TaskbarCreated){
			InjectClockHook(hwnd);
		}
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}

/*---------------------------------------------------------
-- show a message when TClock failed to customize the clock
---------------------------------------------------------*/
void InitError(int n)
{
	wchar_t msg[160];
	
	wsprintf(msg, FMT("%s: %d"), MyString(IDS_NOTFOUNDCLOCK), n);
	api.Message(NULL, msg, L"Error", MB_OK|MB_SETFOREGROUND, MB_ICONEXCLAMATION);
}
/*---------------------------------------------------------
---- Main Timer -------------------------------------------
---- synchronize, alarm, timer, execute Desktop Calendar...
---------------------------------------------------------*/
//================================================================================================
//-----------------------------------+++--> Values Above Are Required by Main Timer Function Below:
void OnTimerMain(HWND hwnd)   //------------------------------------------------------------+++-->
{
	time_t ts;
	
	OnTimerTimer(hwnd); // timer.c
	
	ts = time(NULL);
	OnTimerAlarm(hwnd, ts); // alarm.c
}
//================================================================================================
//----------+++--> Make Background of Desktop Icon Text Labels Transparent (For Windows 2000 Only):
#ifdef WIN2K_COMPAT
void SetDesktopIconTextBk(int enable)   //---------------------------------------------------+++-->
{
	COLORREF col;
	HWND hwnd;
	
	hwnd = FindWindowA("Progman", "Program Manager");
	if(!hwnd)
		return;
	hwnd = GetWindow(hwnd, GW_CHILD);
	hwnd = GetWindow(hwnd, GW_CHILD);
	while(hwnd) {
		char s[80];
		GetClassNameA(hwnd, s, _countof(s));
		if(strcmp(s, "SysListView32") == 0)
			break;
		hwnd = GetWindow(hwnd, GW_HWNDNEXT);
	}
	if(!hwnd) return;
	
	if(enable) {
		col = CLR_NONE;
	} else {
		if(ListView_GetTextBkColor(hwnd) != CLR_NONE)
			return;
		col = GetSysColor(COLOR_DESKTOP);
	}
	
	ListView_SetTextBkColor(hwnd, col);
	ListView_RedrawItems(hwnd, 0, ListView_GetItemCount(hwnd));
	
	hwnd = GetParent(hwnd);
	hwnd = GetWindow(hwnd, GW_CHILD);
	while(hwnd) {
		InvalidateRect(hwnd, NULL, TRUE);
		hwnd = GetWindow(hwnd, GW_HWNDNEXT);
	}
}
#endif // WIN2K_COMPAT
//================================================================================================
//-----------------------+++--> Go Find the Default Windows Clock Window - So We Can Assimilate it: (was this at anypoint from Windows itself?)
//void FindTrayServer()   //---------------------------------------------------------+++-->
//{ // Redux: dunno what this is.. not needed
//	HWND hwndTrayServer = FindWindow("Shell_TrayWnd", "CTrayServer");
//	if(hwndTrayServer > 0) SendMessage(hwndTrayServer, WM_CLOSE, 0, 0);
//}
