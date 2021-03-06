// dui-demo.cpp : main source file
//

#include "stdafx.h"
#include "MainDlg.h"
#include "MySkin.h"
#include "MenuWndHook2.h"
#include "WndShadow2.h"
#include "include/internal/cef_win.h"
#include "include/internal/cef_ptr.h"
#include "CefRealWndHandler.h"
#include "appshell/config.h"

#include "appshell/cefclient.h"


//从PE文件加载，注意从文件加载路径位置
#define RES_TYPE 1
// #define RES_TYPE 1  //从PE资源中加载UI资源

#ifdef _DEBUG
#define SYS_NAMED_RESOURCE _T("soui-sys-resourced.dll")
#else
#define SYS_NAMED_RESOURCE _T("soui-sys-resource.dll")
#endif

//定义唯一的一个R,UIRES对象,ROBJ_IN_CPP是resource.h中定义的宏。
ROBJ_IN_CPP


// Global Variables:
DWORD            g_appStartupTime;
HINSTANCE        hInst;                     // current instance
HACCEL           hAccelTable;
std::wstring     gFilesToOpen;              // Filenames passed as arguments to app

HWND g_main_hwnd;
HANDLE m_timerHandle = NULL;

HWND hMessageWnd = NULL;
HWND CreateMessageWindow(HINSTANCE hInstance);
LRESULT CALLBACK MessageWndProc(HWND, UINT, WPARAM, LPARAM);
int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int /*nCmdShow*/)
{
	HRESULT hRes = OleInitialize(NULL);
	SASSERT(SUCCEEDED(hRes));
	hInst = hInstance;
	CWndShadow2::Initialize(hInst);
	int nRet = 0;

	SComMgr *pComMgr = new SComMgr;

	//将程序的运行路径修改到项目所在目录所在的目录
	TCHAR szCurrentDir[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, szCurrentDir, sizeof(szCurrentDir));
	LPTSTR lpInsertPos = _tcsrchr(szCurrentDir, _T('\\'));
	_tcscpy(lpInsertPos + 1, _T("..\\Biu"));
	SetCurrentDirectory(szCurrentDir);
	{
		BOOL bLoaded = FALSE;
		CAutoRefPtr<SOUI::IImgDecoderFactory> pImgDecoderFactory;
		CAutoRefPtr<SOUI::IRenderFactory> pRenderFactory;
		//bLoaded = pComMgr->CreateRender_GDI((IObjRef**)&pRenderFactory);
		bLoaded = pComMgr->CreateRender_Skia((IObjRef**)&pRenderFactory);
		SASSERT_FMT(bLoaded, _T("load interface [render] failed!"));
		bLoaded = pComMgr->CreateImgDecoder((IObjRef**)&pImgDecoderFactory);
		SASSERT_FMT(bLoaded, _T("load interface [%s] failed!"), _T("imgdecoder"));

		pRenderFactory->SetImgDecoderFactory(pImgDecoderFactory);
		SApplication *theApp = new SApplication(pRenderFactory, hInstance);
		theApp->RegisterSkinClass<MenuArrowSkin>();

		//向SApplication系统中注册由外部扩展的控件及SkinObj类
		SWkeLoader wkeLoader;
		if (wkeLoader.Init(_T("wke.dll")))
		{
			theApp->RegisterWindowClass<SWkeWebkit>();//注册WKE浏览器
		}

		//HKEY_CURRENT_USER\Software\Microsoft\Internet Explorer\Main\FeatureControl\FEATURE_BROWSER_EMULATION
		theApp->RegisterWindowClass<SIECtrl>();//注册IECtrl

		//从DLL加载系统资源
		HMODULE hModSysResource = LoadLibrary(SYS_NAMED_RESOURCE);
		if (hModSysResource)
		{
			CAutoRefPtr<IResProvider> sysResProvider;
			CreateResProvider(RES_PE, (IObjRef**)&sysResProvider);
			sysResProvider->Init((WPARAM)hModSysResource, 0);
			theApp->LoadSystemNamedResource(sysResProvider);
			FreeLibrary(hModSysResource);
		}
		else
		{
			SASSERT(0);
		}

		CAutoRefPtr<IResProvider>   pResProvider;
#if (RES_TYPE == 0)
		CreateResProvider(RES_FILE, (IObjRef**)&pResProvider);
		if (!pResProvider->Init((LPARAM)_T("uires"), 0))
		{
			SASSERT(0);
			return 1;
		}
#else 
		CreateResProvider(RES_PE, (IObjRef**)&pResProvider);
		pResProvider->Init((WPARAM)hInstance, 0);
#endif

		theApp->InitXmlNamedID(namedXmlID, ARRAYSIZE(namedXmlID), TRUE);
		theApp->AddResProvider(pResProvider);
		//加载LUA脚本模块。
		CAutoRefPtr<IScriptModule> pScriptLua;
		bLoaded = pComMgr->CreateScrpit_Lua((IObjRef**)&pScriptLua);
		SASSERT_FMT(bLoaded, _T("load interface [%s] failed!"), _T("script_lua"));

		//加载多语言翻译模块。
		CAutoRefPtr<ITranslatorMgr> trans;
		bLoaded = pComMgr->CreateTranslator((IObjRef**)&trans);
		SASSERT_FMT(bLoaded, _T("load interface [%s] failed!"), _T("translator"));
		if (trans)
		{//加载语言翻译包
			theApp->SetTranslator(trans);
			pugi::xml_document xmlLang;
			if (theApp->LoadXmlDocment(xmlLang, _T("lang_cn"), _T("translator")))
			{
				CAutoRefPtr<ITranslator> langCN;
				trans->CreateTranslator(&langCN);
				langCN->Load(&xmlLang.child(L"language"), 1);//1=LD_XML
				trans->InstallTranslator(langCN);
			}
		}






		// provide CEF with command-line arguments.
		CefMainArgs main_args(hInstance);

		// CefChatApp implements application-level callbacks ,it will create the first browser  
		// instance in OnContextInitialized() after CEF has initialized.

		CefRefPtr<ClientApp> app(new ClientApp);

		int exit_code = CefExecuteProcess(main_args, app.get(), NULL);
		if (exit_code > 0) {
			return exit_code;
		}

		// Specify CEF global settings here
		CefSettings settings;
		settings.no_sandbox = true;
		settings.multi_threaded_message_loop = false;
		CefInitialize(main_args, settings, app, NULL);

		hMessageWnd = CreateMessageWindow(hInstance);

		
		void CALLBACK TimerProc(void* lpParametar,
			BOOLEAN TimerOrWaitFired);
		BOOL success = ::CreateTimerQueueTimer(
			&m_timerHandle,
			NULL,
			TimerProc,
			hMessageWnd,
			0,
			1000 / 80,
			WT_EXECUTEINTIMERTHREAD);








		CCefRealWndHandler*  pRealWndHandler = new CCefRealWndHandler();
		theApp->SetRealWndHandler(pRealWndHandler);
		pRealWndHandler->Release();







		// BLOCK: Run application
		{


			CMenuWndHook2::InstallHook(hInst, L"menu_border");
			CMainDlg dlgMain;
			g_main_hwnd = dlgMain.Create(NULL, (DWORD)WS_POPUP|WS_CLIPCHILDREN, 0UL);
			dlgMain.SendMessage(WM_INITDIALOG);
			dlgMain.CenterWindow(dlgMain.m_hWnd);
			dlgMain.ShowWindow(SW_SHOWNORMAL);

			//SetParent( dlgMain.m_hWnd, hMessageWnd);

			nRet = theApp->Run(dlgMain.m_hWnd);

		}

		delete theApp;
	}

	delete pComMgr;
	::DestroyWindow(hMessageWnd);
	hMessageWnd = NULL;
	CefShutdown();
	if (m_timerHandle) {
		DeleteTimerQueueTimer(NULL, m_timerHandle, INVALID_HANDLE_VALUE);
	}

	OleUninitialize();
	return nRet;
}


void CALLBACK TimerProc(void* lpParametar,
	BOOLEAN TimerOrWaitFired)
{
	// This is used only to call QueueTimerHandler
	// Typically, this function is static member of CTimersDlg
	//CTimersDlg* obj = (CTimersDlg*)lpParametar;
	//obj->QueueTimerHandler();
	HWND hwnd = (HWND)lpParametar;

	::PostMessage(hwnd, WM_CEF_MSG_LOOP_WORK, 0, 0);


}

HWND CreateMessageWindow(HINSTANCE hInstance) {
	static const wchar_t kWndClass[] = L"ClientMessageWindow";

	WNDCLASSEX wc = { 0 };
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = MessageWndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = kWndClass;
	wc.style = CS_VREDRAW | CS_HREDRAW; //窗口风格
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	RegisterClassEx(&wc);
	
	HWND hWnd = CreateWindowEx(0, kWndClass, L"CEFMessageWnd",   0,0, 0, 0, 0, HWND_MESSAGE, NULL,
		hInstance, 0);

	
	return hWnd;
}




LRESULT CALLBACK MessageWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	LRESULT lr = 0;
	switch (message)
	{
		case WM_COMMAND:
		{
			int wmId = LOWORD(wParam);
			switch (wmId)
			{
				case ID_QUIT:
				{
					PostQuitMessage(0);
					return 0;
				}
			}
			break;
		}
		case WM_CEF_MSG_LOOP_WORK: {
			CefDoMessageLoopWork();
			break;
		}

		
		
	
	}
	lr = DefWindowProc(hWnd,message, wParam, lParam);
	// post default message processing
	return lr;
}

