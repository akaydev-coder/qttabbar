//    This file is part of QTTabBar, a shell extension for Microsoft
//    Windows Explorer.
//    Copyright (C) 2007-2021  Quizo, Paul Accisano
//
//    QTTabBar is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    QTTabBar is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with QTTabBar.  If not, see <http://www.gnu.org/licenses/>.

#include <Windows.h>
#include <ShObjIdl.h>
#include <Shlobj.h>
#include <UIAutomationCore.h>
#include <algorithm>
#include <time.h>
#include <map>

//GDI ��� Using GDI
#include <comdef.h>
#include <gdiplus.h>
#include <Shlwapi.h>
#include <string>
#include <vector>
#include <cmath>


#pragma comment(lib, "GdiPlus.lib")

#include "CComPtr.h"
#include "..\MinHook\MinHook.h"


//AlphaBlend
#pragma comment(lib, "Msimg32.lib")

#include <io.h>
#pragma comment(lib, "shlwapi.lib")

using namespace Gdiplus;


void Log(std::wstring log)
{
	OutputDebugStringW((L"\n[Debug]: " + log).c_str());
}

int round_int( double r ) {
    return (r > 0.0) ? (r + 0.5) : (r - 0.5);
}


void Box(LPCWSTR lpText)
{
	// std::round_indeterminate
	 // MessageBoxW(0, lpText, L"Box", MB_ICONERROR);
}


#define Box2(content) {                                                            \
    MessageBoxW(0, L"content", L"Box2", MB_ICONERROR); \
}

void Box1(LPCWSTR lpText)
{
	MessageBoxW(0, lpText, L"Box1", MB_ICONERROR);
}

void Log1(std::wstring log)
{
	//CString strTempPath;
    //::GetTempPath(MAX_PATH, strTempPath.GetBuffer(MAX_PATH));
    LPCWSTR strLogFile = L"c:/Log.txt";
    HANDLE hFile = INVALID_HANDLE_VALUE;
    DWORD dwBytesWritten = 0;
    BOOL bErrorFlag = FALSE;
    OVERLAPPED strOverlapped = {};
    strOverlapped.Offset = 0xFFFFFFFF;
    strOverlapped.OffsetHigh= 0xFFFFFFFF;
    hFile = CreateFile(strLogFile, GENERIC_READ| GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile== INVALID_HANDLE_VALUE)
    {
        return ;
    }
    char TimeMessage[MAX_PATH] = { 0 };
    SYSTEMTIME st;
    ::GetLocalTime(&st);
    char szTime[26] = { 0 };
    sprintf_s(szTime, "%04d-%02d-%02d %02d:%02d:%02d %d ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    sprintf_s(TimeMessage, "%s: %s\n", szTime,log);
    DWORD dwBytesToWrite = (DWORD)strlen(TimeMessage);
    bErrorFlag = WriteFile(hFile, TimeMessage, dwBytesToWrite, NULL, &strOverlapped);
    if (bErrorFlag==FALSE)
    {
        return ;
    }
    CloseHandle(hFile);
}


// Hook declaration macro
#define DECLARE_HOOK(id, ret, name, params)                                         \
    typedef ret (WINAPI *__TYPE__##name)params; /* Function pointer type        */  \
    ret WINAPI Detour##name params;             /* Detour function              */  \
    __TYPE__##name fp##name = NULL;             /* Pointer to original function */  \
    const int hook##name = id;                  /* Hook ID                      */ 

// Hook creation macros
#define CREATE_HOOK(address, name) {                                                            \
    MH_STATUS ret = MH_CreateHook(address, &Detour##name, reinterpret_cast<void**>(&fp##name)); \
    if(ret == MH_OK) ret = MH_EnableHook(address);                                              \
	if(ret != MH_OK) {  Box(L"MH_CreateHook fail"); }  \
    callbacks.fpHookResult(hook##name, ret);                                                    \
}
#define CREATE_COM_HOOK(punk, idx, name) \
    CREATE_HOOK((*(void***)((IUnknown*)(punk)))[idx], name)

// A few undocumented interfaces and classes, of which we only really need the IIDs.
MIDL_INTERFACE("0B907F92-1B63-40C6-AA54-0D3117F03578") IListControlHost     : public IUnknown {};
MIDL_INTERFACE("66A9CB08-4802-11d2-A561-00A0C92DBFE8") ITravelLog           : public IUnknown {};
MIDL_INTERFACE("3050F679-98B5-11CF-BB82-00AA00BDCE0B") ITravelLogEx         : public IUnknown {};
MIDL_INTERFACE("7EBFDD87-AD18-11d3-A4C5-00C04F72D6B8") ITravelLogEntry      : public IUnknown {};
MIDL_INTERFACE("489E9453-869B-4BCC-A1C7-48B5285FD9D8") ICommonExplorerHost  : public IUnknown {};
MIDL_INTERFACE("A86304A7-17CA-4595-99AB-523043A9C4AC") IExplorerFactory 	: public IUnknown {};
MIDL_INTERFACE("E93D4057-B9A2-42A5-8AF8-E5BBF177D365") IShellNavigationBand : public IUnknown {};
MIDL_INTERFACE("596742A5-1393-4E13-8765-AE1DF71ACAFB") CBreadcrumbBar {};
MIDL_INTERFACE("93A56381-E0CD-485A-B60E-67819E12F81B") CExplorerFactoryServer {};

// Win7's IShellBrowserService interface, of which we only need one function.
MIDL_INTERFACE("DFBC7E30-F9E5-455F-88F8-FA98C1E494CA")
IShellBrowserService_7 : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE Unused0() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTravelLog(ITravelLog** ppTravelLog) = 0;
};

// Vista's IShellBrowserService.
MIDL_INTERFACE("42DAD0E2-9B43-4E7A-B9D4-E6D1FF85D173")
IShellBrowserService_Vista : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE Unused0() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unused1() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unused2() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unused3() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTravelLog(ITravelLog** ppTravelLog) = 0;
};

// Hooks
/*__in     REFCLSID rclsid, 
                           __in_opt LPUNKNOWN pUnkOuter,
                           __in     DWORD dwClsContext, 
                           __in     REFIID riid, 
                           __deref_out LPVOID FAR* ppv*/
DECLARE_HOOK( 0, HRESULT, CoCreateInstance, (REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID FAR* ppv))
DECLARE_HOOK( 1, HRESULT, RegisterDragDrop, (HWND hwnd, LPDROPTARGET pDropTarget))
DECLARE_HOOK( 2, HRESULT, SHCreateShellFolderView, (const SFV_CREATE* pcsfv, IShellView** ppsv))


DECLARE_HOOK( 3, HRESULT, BrowseObject, (IShellBrowser* _this, PCUIDLIST_RELATIVE pidl, UINT wFlags))
DECLARE_HOOK( 4, HRESULT, CreateViewWindow3, (IShellView3* _this, IShellBrowser* psbOwner, IShellView* psvPrev, SV3CVW3_FLAGS dwViewFlags, FOLDERFLAGS dwMask, FOLDERFLAGS dwFlags, FOLDERVIEWMODE fvMode, const SHELLVIEWID* pvid, const RECT* prcView, HWND* phwndView))
DECLARE_HOOK( 5, HRESULT, MessageSFVCB, (IShellFolderViewCB* _this, UINT uMsg, WPARAM wParam, LPARAM lParam))
DECLARE_HOOK( 6, LRESULT, UiaReturnRawElementProvider, (HWND hwnd, WPARAM wParam, LPARAM lParam, IRawElementProviderSimple* el))
DECLARE_HOOK( 7, HRESULT, QueryInterface, (IRawElementProviderSimple* _this, REFIID riid, void** ppvObject))
DECLARE_HOOK( 8, HRESULT, TravelToEntry, (ITravelLogEx* _this, IUnknown* punk, ITravelLogEntry* ptle))
DECLARE_HOOK( 9, HRESULT, OnActivateSelection, (IListControlHost* _this, DWORD dwModifierKeys))
DECLARE_HOOK(10, HRESULT, SetNavigationState, (IShellNavigationBand* _this, unsigned long state))
DECLARE_HOOK(11, HRESULT, ShowWindow_Vista, (IExplorerFactory* _this, PCIDLIST_ABSOLUTE pidl, DWORD flags, DWORD mystery1, DWORD mystery2, POINT pt))
DECLARE_HOOK(11, HRESULT, ShowWindow_7, (ICommonExplorerHost* _this, PCIDLIST_ABSOLUTE pidl, DWORD flags, POINT pt, DWORD mystery))
DECLARE_HOOK(12, HRESULT, UpdateWindowList, (/*IShellBrowserService*/ IUnknown* _this))


// ���ñ���ͼƬ�� hook ���� start 
DECLARE_HOOK(13, HWND, CreateWindowExW, (
    DWORD     dwExStyle,
    LPCWSTR   lpClassName,
    LPCWSTR   lpWindowName,
    DWORD     dwStyle,
    int       X,
    int       Y,
    int       nWidth,
    int       nHeight,
    HWND      hWndParent,
    HMENU     hMenu,
    HINSTANCE hInstance,
    LPVOID    lpParam
))


DECLARE_HOOK(14, BOOL, DestroyWindow, (HWND))
DECLARE_HOOK(15, HDC, BeginPaint, (HWND hWnd, LPPAINTSTRUCT lpPaint))

/*
__in HDC hDC,
__in CONST RECT *lprc,
__in HBRUSH hbr
*/
DECLARE_HOOK(16, int, FillRect, (HDC hDC, const RECT* lprc, HBRUSH hbr))
DECLARE_HOOK(17, HDC, CreateCompatibleDC, (HDC hDC))
// ���ñ���ͼƬ�� hook ���� end 







// Messages
unsigned int WM_REGISTERDRAGDROP;
unsigned int WM_NEWTREECONTROL;
unsigned int WM_BROWSEOBJECT;
unsigned int WM_HEADERINALLVIEWS;
unsigned int WM_LISTREFRESHED;
unsigned int WM_ISITEMSVIEW;
unsigned int WM_ACTIVATESEL;
unsigned int WM_BREADCRUMBDPA;
unsigned int WM_CHECKPULSE;

// Callback struct
struct CallbackStruct {
    void (*fpHookResult)(int hookId, int retcode);
    bool (*fpNewWindow)(LPCITEMIDLIST pIDL);
};
CallbackStruct callbacks;

// Other stuff
HMODULE hModAutomation = NULL;
FARPROC fpRealRREP = NULL;
FARPROC fpRealCI = NULL;


//ȫ�ֱ���
#pragma region GlobalVariable

/*GDI Bitmap*/
class BitmapGDI
{
public:
	BitmapGDI(std::wstring path);
	~BitmapGDI();

	HDC pMem ;
	HBITMAP pBmp ;
	SIZE Size;
	Gdiplus::Bitmap* src;
};

HMODULE g_hModule = NULL;           //ȫ��ģ���� Global module handle
bool m_isInitHook = false;          //Hook��ʼ����־ Hook init flag

ULONG_PTR m_gdiplusToken;           //GDI��ʼ����־ GDI Init flag

struct MyData
{
    HWND hWnd ;
    HDC hDC ;
    SIZE size ;
    int ImgIndex ;
};
//First ThreadID
std::map<DWORD, MyData> m_duiList;//dui����б� dui handle list

struct Config
{
    /* 0 = Left top
    *  1 = Right top
    *  2 = Left bottom
    *  3 = Right bottom
    *  4 = Center
    *  5 = Zoom
    *  6 = Zoom Fill
    */
    int imgPosMode ;                 //ͼƬ��λ��ʽ Image position mode type
    bool isRandom ;               //�����ʾͼƬ Random pictures
    BYTE imgAlpha ;                //ͼƬ͸���� Image alpha
    std::vector<BitmapGDI*> imageList;  //����ͼ�б� background image list
} m_config;                             //������Ϣ config

#pragma endregion

// ��ȡ DLL �ļ���Ŀ¼
std::wstring GetCurDllDir()
{
	wchar_t sPath[MAX_PATH];
	GetModuleFileNameW(g_hModule, sPath, MAX_PATH);
	std::wstring path = sPath;
	path = path.substr(0, path.rfind(L"\\"));
	return path;
}


// �ж��ļ��Ƿ����
bool FileIsExist(std::wstring FilePath)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	hFind = FindFirstFileW(FilePath.c_str(), &FindFileData);
	if (hFind != INVALID_HANDLE_VALUE) {
		FindClose(hFind);
		return true;
	}
	return false;
}
// ��ȡ INI ����������
std::wstring GetIniString(std::wstring FilePath, std::wstring AppName, std::wstring KeyName)
{
	if (FileIsExist(FilePath)) {
		HANDLE pFile = CreateFileW(FilePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		LARGE_INTEGER fileSize;
		GetFileSizeEx(pFile, &fileSize);

		wchar_t* data = new wchar_t[fileSize.QuadPart];
		ZeroMemory(data, sizeof(wchar_t) * fileSize.QuadPart);
		GetPrivateProfileStringW(AppName.c_str(), KeyName.c_str(), NULL, data, (DWORD)fileSize.QuadPart, FilePath.c_str());

		std::wstring ret = data;
		delete[] data;

		CloseHandle(pFile);
		return ret;
	}
	return std::wstring();
}

// �����ļ�
void EnumFiles(std::wstring path, std::wstring append, std::vector<std::wstring>& fileList)
{
	
	//�ļ���� 
	// intptr_t  hFile = 0;
	// //�ļ���Ϣ 
	// struct _wfinddata_t fileinfo;
	std::wstring p;
	// std::string inPath = "./*.png"; // ��ǰĿ¼������
	// auto lp_text = p.assign(path).append(L"\\" + append).c_str();
	auto lp_text = p.assign(path).append(L"\\" + append).c_str();
	// Box1(lp_text);
	WIN32_FIND_DATA fileinfo2;
	//�ļ����
	HANDLE myHandle = INVALID_HANDLE_VALUE;
	// if ((hFile = _findfirst(lp_text, &fileinfo)) != -1)
	if ((myHandle = FindFirstFile(lp_text, &fileinfo2)) != INVALID_HANDLE_VALUE)
	{
		// Box1(L"FOUND");
		do
		{
			// if (!(fileinfo.attrib ))
			if (myHandle != INVALID_HANDLE_VALUE)
			{
				/*std::wstring path_ = path + L"\\";
				path_ += fileinfo.name;
				Box1( path_.c_str() );
				fileList.push_back(path_);*/

				std::wstring path_ = path + L"\\";
				path_ += fileinfo2.cFileName;
				// Box1( path_.c_str() );
				fileList.push_back(path_);
			}
		// } while (_wfindnext(hFile, &fileinfo) == 0);
		} while (FindNextFile(myHandle, &fileinfo2));
		FindClose(myHandle);
		// _findclose(hFile);
	}
}

void LoadSettings(bool loadimg)
{
    //�ͷž���Դ
    if (loadimg) {
		/*for (auto image_list : m_config.imageList)
		{
			delete &image_list;
		}*/
        m_config.imageList.clear();
    }

    //�������� Load config
    std::wstring cfgPath = GetCurDllDir() + L"\\config.ini";

	m_config.isRandom = false;

	Box(cfgPath.c_str() );

    m_config.isRandom = GetIniString(cfgPath, L"image", L"random") == L"true" ? true : false;
	// m_config.isRandom = false;

    Box(L"Load random ");

    //ͼƬ��λ��ʽ
    /* 0 = Left top
    *  1 = Right top
    *  2 = Left bottom
    *  3 = Right bottom
    *  4 = Center
    *  5 = Zoom
    *  6 = Zoom Fill
    */
    std::wstring str = GetIniString(cfgPath, L"image", L"posType");
    // std::wstring str = L"6";
    if (str == L"") str = L"0";
    m_config.imgPosMode = std::stoi(str);
    if (m_config.imgPosMode < 0 || m_config.imgPosMode > 6)
        m_config.imgPosMode = 6;

    // m_config.imgPosMode = 6;

    //ͼƬ͸����
    str = GetIniString(cfgPath, L"image", L"imgAlpha");
    // str = L"imgAlpha";
	
	// Box(str.c_str());
    if (str == L"")
        m_config.imgAlpha = 255;
    else
    {
        int alpha = std::stoi(str);
        if (alpha > 255) alpha = 255;
        if (alpha < 0) alpha = 0;
        m_config.imgAlpha = (BYTE)alpha;
    }
	// m_config.imgAlpha = 255;
	Box(L"shit");
    //����ͼ�� Load Image
    if (loadimg) {
        std::wstring imgDir = GetCurDllDir() + L"\\Image";
        // std::wstring imgPath = L"C:\\ProgramData\\QTTabBar\\Image";

        Box(imgDir.c_str());

		// ͼƬ�б�
		std::vector<std::wstring> fileList;

		std::wstring imgPath = GetIniString(cfgPath, L"image", L"imgPath");
		if (FileIsExist(imgPath)) { // ���ָ�����ļ����� �����ָ�����ļ����ر����
			fileList.push_back(imgPath);
			// �Զ���ͼƬ��ر����
			m_config.isRandom = false;
		} else  if (FileIsExist(imgDir)) // ���Ŀ¼ C:\\ProgramData\\QTTabBar\\Image
        {
         
            EnumFiles(imgDir, L"*.png", fileList);
            EnumFiles(imgDir, L"*.jpg", fileList);
            EnumFiles(imgDir, L"*.jpeg", fileList);

			// ����δ����������һ��Ĭ�ϵ�ͼƬ��ַ C:\\ProgramData\\QTTabBar\\Image\\bgImage.png
			/*if (fileList.size() == 0) {
				// ���Ϊ 0 �Ļ��� ������Զ����ͼƬ
				fileList.push_back(L"C:\\ProgramData\\QTTabBar\\Image\\bgImage.png");
			}*/

			// δ���ص�ͼƬ����Ӱ�����
            if (fileList.size() == 0) {
                /*MessageBoxW(0, 
					L"�ļ���Դ����������Ŀ¼û���ļ��������չ�������κ�Ч��.", 
					imgDir.c_str(), 
					MB_ICONERROR);*/
                return;
            } else {
	            Box(L"enum files right");
			}

			/*BitmapGDI* bmp = new BitmapGDI(L"C:\\ProgramData\\QTTabBar\\Image\\bgImage.png");
                if (bmp->src)
                    m_config.imageList.push_back(bmp);
                else
                    delete bmp;//ͼƬ����ʧ�� load failed*/

			// MessageBoxW(0, L"LoadSettings", L" bpm " + fileList.size(), MB_ICONERROR);
            // Log(L"LoadSettings " + fileList.size() );

        }
        else {  // ��� Image Ŀ¼������
            /*MessageBoxW(0, 
				L"�ļ���Դ����������Ŀ¼�����ڣ������չ�������κ�Ч��.", 
				L"ȱ��ͼƬĿ¼", 
				MB_ICONERROR);*/
            // ���Ŀ¼�����ڣ�������Զ���ͼƬ
        }

		// �ж��Ƿ���ͼƬ�����ʼ��
		if (fileList.size() > 0 )
		{
			for (size_t i = 0; i < fileList.size(); i++)
	        {
	            BitmapGDI* bmp = new BitmapGDI(fileList[i]);
				Box(L"BitmapGDI new success ");
	            if (bmp->src)
	                m_config.imageList.push_back(bmp);
	            else
	                delete bmp;//ͼƬ����ʧ�� load failed

	            /*����� ֻ����һ��
	            * Load only one image non randomly
				*/
	            if (!m_config.isRandom) break;
	        }
		}
		return;
    }
}


// ͼƬ���캯��
BitmapGDI::BitmapGDI(std::wstring path)
{
	//����������Ϊ�˷�ֹ�ļ���ռ��
	FILE* file = nullptr;
	_wfopen_s(&file, path.c_str(), L"rb");
	if (file) {
		fseek(file, 0L, SEEK_END);
		long len = ftell(file);
		rewind(file);
		BYTE* pdata = new BYTE[len];
		fread(pdata, 1, len, file);
		fclose(file);

		// IStream* stream = CreateStreamOnHGlobal(NULL, TRUE, &istream);
		// 	// SHCreateMemStream(pdata, len);
		// delete[] pdata;

		IStream* stream = SHCreateMemStream(pdata, len);
		delete[] pdata;
		// IStream* stream = NULL;
		// Box1(L"load BYTES SUC");

		src = Gdiplus::Bitmap::FromStream(stream);
		if ( true )
		{
			// src = Gdiplus::Bitmap::FromStream(stream);
			if (src) {
				// Box1(L"src right");
				pMem = CreateCompatibleDC(0);
				// Size = new Size();
				Size.cx = src->GetWidth();
				Size.cy = src->GetHeight();

				// Size = { (LONG)src->GetWidth(), (LONG)src->GetHeight() };
				src->GetHBITMAP(0, &pBmp);
				SelectObject(pMem, pBmp);
				stream->Release();
				// Log(L"BitmapGDI load suc" );
			}
			else if (stream)
				stream->Release();
		}
	}
}

// ͼƬ������������
BitmapGDI::~BitmapGDI()
{
	if (src)
		delete src;
	if (pMem)
		DeleteDC(pMem);
	if (pBmp)
		DeleteObject(pBmp);
}




extern "C" __declspec(dllexport) int Initialize(CallbackStruct* cb);
extern "C" __declspec(dllexport) int Dispose();
extern "C" __declspec(dllexport) int InitShellBrowserHook(IShellBrowser* psb);

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_DETACH:
            return Dispose();

        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }

	if (ul_reason_for_call == DLL_PROCESS_ATTACH && !g_hModule) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);

        //��ֹ��ĳ����������
        wchar_t pName[MAX_PATH];
        GetModuleFileNameW(NULL, pName, MAX_PATH);

        //������תСд
        std::wstring path = std::wstring(pName);
        std::wstring name = path.substr(path.length() - 12, 12);
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        // if (name == L"explorer.exe") InjectionEntryPoint();
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH)
    {
        /*for (auto& bmp : m_config.imageList)
        {
            delete bmp;
        }*/


		std::vector<BitmapGDI*>::iterator it;
		for(it=m_config.imageList.begin();it!=m_config.imageList.end();it++)
		{
			delete &it;
		}
   
        m_config.imageList.clear();
        m_duiList.clear();
    }
    return true;
}

int Initialize(CallbackStruct* cb) {

    volatile static long initialized;
	
	// Log(L"Initialize");
	// MessageBoxW(0, L"Initialize", L"Initialize", MB_ICONERROR);
    if(InterlockedIncrement(&initialized) != 1) {
        // Return if another thread has beaten us here.
        initialized = 1;
        return MH_OK;
    }
	

    //�������������
    srand((int)time(0));
	
	
	Box(L"MH_Initialize");
    // Initialize MinHook.
    MH_STATUS ret = MH_Initialize();
    if(ret != MH_OK && ret != MH_ERROR_ALREADY_INITIALIZED) return ret;

    // Store the callback struct
    callbacks = *cb;

    // Register the messages.
    // WM_REGISTERDRAGDROP = RegisterWindowMessageA("QTTabBar_RegisterDragDrop");
    // WM_NEWTREECONTROL   = RegisterWindowMessageA("QTTabBar_NewTreeControl");
    // WM_BROWSEOBJECT     = RegisterWindowMessageA("QTTabBar_BrowseObject");
    // WM_HEADERINALLVIEWS = RegisterWindowMessageA("QTTabBar_HeaderInAllViews");
    // WM_LISTREFRESHED    = RegisterWindowMessageA("QTTabBar_ListRefreshed");
    // WM_ISITEMSVIEW      = RegisterWindowMessageA("QTTabBar_IsItemsView");
    // WM_ACTIVATESEL      = RegisterWindowMessageA("QTTabBar_ActivateSelection");
    // WM_BREADCRUMBDPA    = RegisterWindowMessageA("QTTabBar_BreadcrumbDPA");
    // WM_CHECKPULSE       = RegisterWindowMessageA("QTTabBar_CheckPulse");

    // Create and enable the CoCreateInstance, RegisterDragDrop, and SHCreateShellFolderView hooks.
	Box(L"CREATE_HOOK start");
    // CREATE_HOOK(&CoCreateInstance, CoCreateInstance);
    // CREATE_HOOK(&RegisterDragDrop, RegisterDragDrop);
    // CREATE_HOOK(&SHCreateShellFolderView, SHCreateShellFolderView);
	
	CREATE_HOOK(&CreateWindowExW, CreateWindowExW);
    CREATE_HOOK(&DestroyWindow, DestroyWindow);
    CREATE_HOOK(&BeginPaint, BeginPaint);
    CREATE_HOOK(&FillRect, FillRect);
    CREATE_HOOK(&CreateCompatibleDC, CreateCompatibleDC);
	Box(L"CREATE_HOOK end");
	LoadSettings(true);
	Box(L"LoadSettings end");

    /*
    // Create and enable the UiaReturnRawElementProvider hook (maybe)
    hModAutomation = LoadLibraryA("UIAutomationCore.dll");
    if(hModAutomation != NULL) {
        fpRealRREP = GetProcAddress(hModAutomation, "UiaReturnRawElementProvider");
        if(fpRealRREP != NULL) {
            CREATE_HOOK(fpRealRREP, UiaReturnRawElementProvider);
        }
    }
    
    // Create an instance of the breadcrumb bar so we can hook it.
    CComPtr<IShellNavigationBand> psnb;
    if(psnb.Create(__uuidof(CBreadcrumbBar), CLSCTX_INPROC_SERVER)) {
        CREATE_COM_HOOK(psnb, 4, SetNavigationState)
    }


    // Create an instance of CExplorerFactoryServer so we can hook it.
    // The interface in question is different on Vista and 7.
    CComPtr<IUnknown> punk;
    if(punk.Create(__uuidof(CExplorerFactoryServer), CLSCTX_INPROC_SERVER)) {
        CComPtr<ICommonExplorerHost> pceh;
        CComPtr<IExplorerFactory> pef;
        if(pceh.QueryFrom(punk)) {
            CREATE_COM_HOOK(pceh, 3, ShowWindow_7)
        }
        else if(pef.QueryFrom(punk)) {
            CREATE_COM_HOOK(pef, 3, ShowWindow_Vista)
        }
    }*/
    return MH_OK;
}


int InitShellBrowserHook(IShellBrowser* psb) {
    volatile static long initialized;
    if(InterlockedIncrement(&initialized) != 1) {
        // Return if another thread has beaten us here.
        initialized = 1;
        return MH_OK;
    }

    // Create the BrowseObject hook
    CREATE_COM_HOOK(psb, 11, BrowseObject);

    // Vista and 7 have different IShellBrowserService interfaces.
    // Hook UpdateWindowList in whichever one we have, and get the TravelLog.
    CComPtr<IShellBrowserService_7> psbs7;
    CComPtr<IShellBrowserService_Vista> psbsv;
    CComPtr<ITravelLog> ptl;
    if(psbs7.QueryFrom(psb)) {
        CREATE_COM_HOOK(psbs7, 10, UpdateWindowList);
        psbs7->GetTravelLog(&ptl);
    }
    else if(psbsv.QueryFrom(psb)) {
        CREATE_COM_HOOK(psbsv, 17, UpdateWindowList);
        psbsv->GetTravelLog(&ptl);
    }

    // Hook ITravelLogEx::TravelToEntry to catch the Search band navigation
    if(ptl != NULL) {
        CComPtr<ITravelLogEx> ptlex;
        if(ptlex.QueryFrom(ptl)) {
            CREATE_COM_HOOK(ptlex, 11, TravelToEntry);
        }
    }
    return MH_OK;
}

int Dispose() {
    // Uninitialize MinHook.
    MH_Uninitialize();

    // Free the Automation library
    if(hModAutomation != NULL) {
        FreeLibrary(hModAutomation);
    }

    return S_OK;
}


//Hook��ԭʼ����
#pragma region Original Function

typedef HWND(WINAPI* O_CreateWindowExW)(DWORD, LPCWSTR, LPCWSTR, DWORD,
    int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
O_CreateWindowExW _CreateWindowExW_;

typedef BOOL(WINAPI* O_DestroyWindow)(HWND);
O_DestroyWindow _DestroyWindow_;

typedef HDC(WINAPI* O_BeginPaint)(HWND, LPPAINTSTRUCT);
O_BeginPaint _BeginPaint_;

typedef int(WINAPI* O_FillRect)(HDC, const RECT*, HBRUSH);
O_FillRect _FillRect_;

typedef HDC(WINAPI* O_CreateCompatibleDC)(HDC);
O_CreateCompatibleDC _CreateCompatibleDC_;

#pragma endregion

//////////////////////////////
// Detour Functions
//////////////////////////////
HWND WINAPI DetourCreateWindowExW(
    DWORD     dwExStyle,
    LPCWSTR   lpClassName,
    LPCWSTR   lpWindowName,
    DWORD     dwStyle,
    int       X,
    int       Y,
    int       nWidth,
    int       nHeight,
    HWND      hWndParent,
    HMENU     hMenu,
    HINSTANCE hInstance,
    LPVOID    lpParam
)
{
	HWND hWnd = fpCreateWindowExW(dwExStyle, lpClassName, lpWindowName, dwStyle,
        X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    //HWND hWnd = _CreateWindowExW_(dwExStyle, lpClassName, lpWindowName, dwStyle,
    //    X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
	WCHAR cn_chars[256];
	WCHAR pcn_chars[256];
	std::wstring classname;
	std::wstring parentClassName;

    if (hWnd)
    {
        GetClassName(hWnd, cn_chars, sizeof(cn_chars));
        GetClassName(hWndParent, pcn_chars, sizeof(pcn_chars));
        // ClassName = GetWindowClassName(hWnd);
		classname = std::wstring(cn_chars);
	    parentClassName = std::wstring(pcn_chars);
    }

	// Box(L"GetClassName");

    //explorer window 
    if (classname == L"DirectUIHWND"
        && parentClassName == L"SHELLDLL_DefView")
    {
        //�������Ҹ��� Continue to find parent
        HWND parent = GetParent(hWndParent);
		GetClassName(parent, cn_chars, sizeof(classname));
        classname = std::wstring(cn_chars);

		if (classname == L"ShellTabWindowClass")
        {
            //��¼���б��� Add to list
            MyData data;
            data.hWnd = hWnd;
            auto imgSize = m_config.imageList.size();
            if (m_config.isRandom && imgSize)
            {
                data.ImgIndex = rand() % imgSize;
            } else
            {
	            data.ImgIndex = 0;
            }
            m_duiList[GetCurrentThreadId()] = data;
			// Box1(L" mydata load hWnd suc!");
            // Box1(L"map load suc tid " + GetCurrentThreadId());
        } else
        {
	        // Box1(L" mydata load hWnd fail1!");
        }
    } else
    {
	    // Box1(L" mydata load hWnd fail2!");
    }
	// Log(L"DetourCreateWindowExW" );
    return hWnd;
}

BOOL WINAPI DetourDestroyWindow(HWND hWnd)
{
    //���Ҳ�ɾ���б��еļ�¼ Find and remove from list
    auto iter = m_duiList.find(GetCurrentThreadId());
    if (iter != m_duiList.end())
    {
        if (iter->second.hWnd == hWnd)
        {
	        m_duiList.erase(iter);
        }
    }
    // Box1(L"destory window suc");
    return fpDestroyWindow(hWnd);
}



HDC WINAPI DetourBeginPaint(HWND hWnd, LPPAINTSTRUCT lpPaint)
{
    //��ʼ����DUI���� BeginPaint dui window
    HDC hDC = fpBeginPaint(hWnd, lpPaint);

    auto iter = m_duiList.find(GetCurrentThreadId());

    if (iter != m_duiList.end()) {
        if (iter->second.hWnd == hWnd)
        {
	        // Box1(L" set hdc suc ");
            //��¼���б� Record values to list
            iter->second.hDC = hDC;
        }
    }
    return hDC;
}


int WINAPI DetourFillRect(HDC hDC, const RECT* lprc, HBRUSH hbr)
{
    int ret = fpFillRect(hDC, lprc, hbr);


	// Box1(L"DetourFillRect in ");
    auto iter = m_duiList.find(GetCurrentThreadId());

	if (iter->second.hDC == hDC )
	{
		// Box1(L" resolve hdc suc" );
	}
    if (iter != m_duiList.end()) {
        if (iter->second.hDC == hDC && m_config.imageList.size())
        {
			// Box1(L" second hdc suc" );
            RECT pRc;
            GetWindowRect(iter->second.hWnd, &pRc);
            SIZE wndSize = { pRc.right - pRc.left, pRc.bottom - pRc.top };

            /*��ͼƬ��λ��ʽ��ͬ ������ڴ�С�ı� ��Ҫȫ���ػ� �����в���
            * Due to different image positioning methods,
            * if the window size changes, you need to redraw, otherwise there will be residues*/
            if ((iter->second.size.cx != wndSize.cx || iter->second.size.cy != wndSize.cy)
                && m_config.imgPosMode != 0) {
                InvalidateRect(iter->second.hWnd, 0, TRUE);
            }
			
			// Box1(L"InvalidateRect suc ");

            //�ü����� Clip rect
            SaveDC(hDC);
            IntersectClipRect(hDC, lprc->left, lprc->top, lprc->right, lprc->bottom);

			// Box1(L"SaveDC IntersectClipRect suc ");
            BitmapGDI* pBgBmp = m_config.imageList[iter->second.ImgIndex];

            //����ͼƬλ�� Calculate picture position
            POINT pos;
            SIZE dstSize = { pBgBmp->Size.cx, pBgBmp->Size.cy };

			// Box1(L"calc pos suc ");
            switch (m_config.imgPosMode)
            {
            case 0://����
                 pos.x = 0;
                pos.y = 0;
                break;
            case 1://����
                pos.x = wndSize.cx - pBgBmp->Size.cx;
                pos.y = 0;
                break;
            case 2://����
                pos.x = 0;
                pos.y = wndSize.cy - pBgBmp->Size.cy;
                break;
            case 3://����
                pos.x = wndSize.cx - pBgBmp->Size.cx;
                pos.y = wndSize.cy - pBgBmp->Size.cy;
                break;
            case 4://���������@ʾ
                pos.x = (wndSize.cx - pBgBmp->Size.cx) >> 1;
                pos.y = (wndSize.cy - pBgBmp->Size.cy) >> 1;
                break;
            case 5://����
                  {
                      int newWidth = wndSize.cx;
                      int newHeight = wndSize.cy;
                      pos.x = 0;
                      pos.y = 0;

                      // dstSize = { newWidth, newHeight };

					 dstSize.cx = newWidth;
                     dstSize.cy = newHeight;
                  }
                  break;
            case 6://���Ų����
                 {
                     /*static auto calcAspectRatio = [](int fromWidth, int fromHeight, int toWidthOrHeight, bool isWidth)
                     {
                         if (isWidth) {
                             return (int)(((float)fromHeight * ((float)toWidthOrHeight / (float)fromWidth)));
                         }
                         else {
                             return (int)(((float)fromWidth * ((float)toWidthOrHeight / (float)fromHeight)));
                         }
                     };*/
					 
                     //���ߵȱ�������
                     // int newWidth = calcAspectRatio(pBgBmp->Size.cx, pBgBmp->Size.cy, wndSize.cy, false);
					 
                     int newWidth =  round_int((float)pBgBmp->Size.cx * ((float)wndSize.cy / (float)pBgBmp->Size.cy));
                     int newHeight = wndSize.cy;

                     pos.x = newWidth - wndSize.cx;
                     pos.x /= 2;//����
                     if (pos.x != 0) pos.x = -pos.x;
                     pos.y = 0;

					
                     //���߲��������� ����
                     if (newWidth < wndSize.cx) {
                         newWidth = wndSize.cx;
                         newHeight = round_int((float)pBgBmp->Size.cy * ((float)wndSize.cx / (float)pBgBmp->Size.cx));
                         // newHeight = calcAspectRatio(pBgBmp->Size.cx, pBgBmp->Size.cy, wndSize.cx, true);
                         pos.x = 0;
                         pos.y = newHeight - wndSize.cy;
                         pos.y /= 2;//����
                         if (pos.y != 0) pos.y = -pos.y;
                     }
                     // dstSize = { newWidth, newHeight };
					 dstSize.cx = newWidth;
                     dstSize.cy = newHeight;
                 }
				 
				// Box1(L"imgPosMode is 6 break");
                break;
            default://Ĭ�J����
                pos.x = wndSize.cx - pBgBmp->Size.cx;
                pos.y = wndSize.cy - pBgBmp->Size.cy;
                break;
            }
			
            /*����ͼƬ Paint image*/
            BLENDFUNCTION bf = { AC_SRC_OVER, 0, m_config.imgAlpha, AC_SRC_ALPHA };
			
			// Box1(L"BLENDFUNCTION suc");
            AlphaBlend(
                hDC, 
                pos.x, 
                pos.y, 
                dstSize.cx, 
                dstSize.cy, 
                pBgBmp->pMem, 
                0, 
                0, 
                pBgBmp->Size.cx, 
                pBgBmp->Size.cy, 
				bf);
			// Box1(L"AlphaBlend suc");
            RestoreDC(hDC, -1);
			// Box1(L"RestoreDC suc");
            iter->second.size = wndSize;
            // Log(L"DrawImage");
        }
    }
    return ret;
}


HDC WINAPI DetourCreateCompatibleDC(HDC hDC)
{
    //�ڻ���DUI֮ǰ �����CreateCompatibleDC �ҵ���
    //CreateCompatibleDC is called before drawing the DUI
    HDC retDC = fpCreateCompatibleDC(hDC);

    auto iter = m_duiList.find(GetCurrentThreadId());
    if (iter != m_duiList.end()) {
        if (iter->second.hDC == hDC)
        {
	        iter->second.hDC = retDC;
	        // Box1(L" second hdc suc");
        }
    }
    return retDC;
}


// The purpose of this hook is to intercept the creation of the NameSpaceTreeControl object, and 
// send a reference to the control to QTTabBar.  We can use this reference to hit test the
// control, which is how opening new tabs from middle-click works.
HRESULT WINAPI DetourCoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID FAR* ppv) {
    HRESULT ret = fpCoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
	// Box(L"DetourCoCreateInstance");
    if(SUCCEEDED(ret) && IsEqualIID(rclsid, CLSID_NamespaceTreeControl)) {
        PostThreadMessage(GetCurrentThreadId(), WM_NEWTREECONTROL, (WPARAM)(*ppv), NULL);
    }  
    return ret;
}

// The purpose of this hook is to allow QTTabBar to insert its IDropTarget wrapper in place of
// the real IDropTarget.  This is much more reliable than using the GetProp method.
HRESULT WINAPI DetourRegisterDragDrop(IN HWND hwnd, IN LPDROPTARGET pDropTarget) {
    LPDROPTARGET* ppDropTarget = &pDropTarget;
    SendMessage(hwnd, WM_REGISTERDRAGDROP, (WPARAM)ppDropTarget, NULL);
    return fpRegisterDragDrop(hwnd, *ppDropTarget);
}

// The purpose of this hook is just to set other hooks.  It is disabled once the other hooks are set.
HRESULT WINAPI DetourSHCreateShellFolderView(const SFV_CREATE* pcsfv, IShellView** ppsv) {
    HRESULT ret = fpSHCreateShellFolderView(pcsfv, ppsv);
    CComPtr<IShellView> psv(*ppsv);
    if(SUCCEEDED(ret) && psv.Implements(IID_CDefView)) {

        CREATE_COM_HOOK(pcsfv->psfvcb, 3, MessageSFVCB);

        CComPtr<IShellView3> psv3;
        if(psv3.QueryFrom(psv)) {
            CREATE_COM_HOOK(psv3, 20, CreateViewWindow3);
        }

        CComPtr<IListControlHost> plch;
        if(plch.QueryFrom(psv)) {
            CREATE_COM_HOOK(plch, 3, OnActivateSelection);
        }

        // Disable this hook, no need for it anymore.
        MH_DisableHook(&SHCreateShellFolderView);
    }
    return ret;
}

// The purpose of this hook is to work around Explorer's BeforeNavigate2 bug.  It allows QTTabBar
// to be notified of navigations before they occur and have the chance to veto them.
HRESULT WINAPI DetourBrowseObject(IShellBrowser* _this, PCUIDLIST_RELATIVE pidl, UINT wFlags) {
    HWND hwnd;
    LRESULT result = 0;
    if(SUCCEEDED(_this->GetWindow(&hwnd))) {
        HWND parent = GetParent(hwnd);
        if(parent != 0) hwnd = parent;
        result = SendMessage(hwnd, WM_BROWSEOBJECT, (WPARAM)(&wFlags), (LPARAM)pidl);
    } 
    return result == 0 ? fpBrowseObject(_this, pidl, wFlags) : S_FALSE;
}

// The purpose of this hook is to enable the Header In All Views functionality, if the user has 
// opted to use it.
HRESULT WINAPI DetourCreateViewWindow3(IShellView3* _this, IShellBrowser* psbOwner, IShellView* psvPrev, SV3CVW3_FLAGS dwViewFlags, FOLDERFLAGS dwMask, FOLDERFLAGS dwFlags, FOLDERVIEWMODE fvMode, const SHELLVIEWID* pvid, const RECT* prcView, HWND* phwndView) {
    HWND hwnd;
    LRESULT result = 0;
    if(psbOwner != NULL && SUCCEEDED(psbOwner->GetWindow(&hwnd))) {
        HWND parent = GetParent(hwnd);
        if(parent != 0) hwnd = parent;
        if(SendMessage(hwnd, WM_HEADERINALLVIEWS, 0, 0) != 0) {
            dwMask |= FWF_NOHEADERINALLVIEWS;
            dwFlags &= ~FWF_NOHEADERINALLVIEWS;
        }
    }
    return fpCreateViewWindow3(_this, psbOwner, psvPrev, dwViewFlags, dwMask, dwFlags, fvMode, pvid, prcView, phwndView);
}

// The purpose of this hook is to notify QTTabBar whenever an Explorer refresh occurs.  This allows
// the search box to be cleared.
HRESULT WINAPI DetourMessageSFVCB(IShellFolderViewCB* _this, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if(uMsg == 0x11 /* SFVM_LISTREFRESHED */ && wParam != 0) {
        PostThreadMessage(GetCurrentThreadId(), WM_LISTREFRESHED, NULL, NULL);
    }
    return fpMessageSFVCB(_this, uMsg, wParam, lParam);
}

// The purpose of this hook is just to set another hook.  It is disabled once the other hook is set.
LRESULT WINAPI DetourUiaReturnRawElementProvider(HWND hwnd, WPARAM wParam, LPARAM lParam, IRawElementProviderSimple* el) {
    if(fpQueryInterface == NULL && (LONG)lParam == OBJID_CLIENT && SendMessage(hwnd, WM_ISITEMSVIEW, 0, 0) == 1) {
        CREATE_COM_HOOK(el, 0, QueryInterface);
        // Disable this hook, no need for it anymore.
        MH_DisableHook(fpRealRREP);
    }
    return fpUiaReturnRawElementProvider(hwnd, wParam, lParam, el);
}

// The purpose of this hook is to work around kb2462524, aka the scrolling lag bug.
HRESULT WINAPI DetourQueryInterface(IRawElementProviderSimple* _this, REFIID riid, void** ppvObject) {
    return IsEqualIID(riid, __uuidof(IRawElementProviderAdviseEvents))
            ? E_NOINTERFACE
            : fpQueryInterface(_this, riid, ppvObject);
}

// The purpose of this hook is to make clearing a search go back to the original directory.
HRESULT WINAPI DetourTravelToEntry(ITravelLogEx* _this, IUnknown* punk, ITravelLogEntry* ptle) {
    CComPtr<IShellBrowser> psb;
    LRESULT result = 0;
    if(punk != NULL && psb.QueryFrom(punk)) {
        HWND hwnd;
        if(SUCCEEDED(psb->GetWindow(&hwnd))) {
            HWND parent = GetParent(hwnd);
            if(parent != 0) hwnd = parent;
            UINT wFlags = SBSP_NAVIGATEBACK | SBSP_SAMEBROWSER;
            result = SendMessage(parent, WM_BROWSEOBJECT, (WPARAM)(&wFlags), NULL);
        }
    }
    return result == 0 ? fpTravelToEntry(_this, punk, ptle) : S_OK;
}

// The purpose of this hook is let QTTabBar handle activating the selection, so that recently
// opened files can be logged (among other features).
HRESULT WINAPI DetourOnActivateSelection(IListControlHost* _this, DWORD dwModifierKeys) {
    CComPtr<IShellView> psv;
    LRESULT result = 0;
    if(psv.QueryFrom(_this)) {
        HWND hwnd;
        if(SUCCEEDED(psv->GetWindow(&hwnd))) {
            result = SendMessage(hwnd, WM_ACTIVATESEL, (WPARAM)(&dwModifierKeys), 0);
        }
    }
    return result == 0 ? fpOnActivateSelection(_this, dwModifierKeys) : S_OK;
}

// The purpose of this hook is to send the Breadcrumb Bar's internal DPA handle to QTTabBar,
// so that we can use it map the buttons to their corresponding IDLs.  This allows middle-click
// on the breadcrumb bar to work.  The DPA handle changes whenever this function is called.
HRESULT WINAPI DetourSetNavigationState(IShellNavigationBand* _this, unsigned long state) {
	HRESULT ret = fpSetNavigationState(_this, state);
    // I find the idea of reading an internal private variable of an undocumented class to
    // be quite unsettling.  Unfortunately, I see no way around it.  It's been in the same
    // location since the first Vista release, so I guess it should be safe...
    HDPA hdpa = (HDPA)(((void**)_this)[6]);
    CComPtr<IOleWindow> pow;
    if(pow.QueryFrom(_this)) {
        HWND hwnd;
        if(SUCCEEDED(pow->GetWindow(&hwnd))) {
            SendMessage(hwnd, WM_BREADCRUMBDPA, NULL, (LPARAM)hdpa);
        }
    }
    return ret;
}

// The purpose of this hook is to alert QTTabBar that a new window is opening, so that we can 
// intercept it if the user has enabled the appropriate option.  The hooked function is different
// on Vista and 7.
HRESULT WINAPI DetourShowWindow_7(ICommonExplorerHost* _this, PCIDLIST_ABSOLUTE pidl, DWORD flags, POINT pt, DWORD mystery) {
    return callbacks.fpNewWindow(pidl) ? S_OK : fpShowWindow_7(_this, pidl, flags, pt, mystery);
}
HRESULT WINAPI DetourShowWindow_Vista(IExplorerFactory* _this, PCIDLIST_ABSOLUTE pidl, DWORD flags, DWORD mystery1, DWORD mystery2, POINT pt) {
    return callbacks.fpNewWindow(pidl) ? S_OK : fpShowWindow_Vista(_this, pidl, flags, mystery1, mystery2, pt);
}

// The SHOpenFolderAndSelectItems function opens an Explorer window and waits for a New Window
// notification from IShellWindows.  The purpose of this hook is to wake up those threads by
// faking such a notification.  It's important that it happens after IShellBrowser::OnNavigate is
// called by the real Explorer window, which happens in IShellBrowserService::UpdateWindowList.
HRESULT WINAPI DetourUpdateWindowList(/* IShellBrowserService */ IUnknown* _this) {
    HRESULT hr = fpUpdateWindowList(_this);
    CComPtr<IShellBrowser> psb;
    LRESULT result = 0;
    if(psb.QueryFrom(_this)) {
        HWND hwnd;
        if(SUCCEEDED(psb->GetWindow(&hwnd))) {
            HWND parent = GetParent(hwnd);
            if(parent != 0) hwnd = parent;
            CComPtr<IDispatch> pdisp;
            if(SendMessage(parent, WM_CHECKPULSE, NULL, (WPARAM)(&pdisp)) != 0 && pdisp != NULL) {
                CComPtr<IShellWindows> psw;
                if(psw.Create(CLSID_ShellWindows, CLSCTX_ALL)) {
                    long cookie;
                    if(SUCCEEDED(psw->Register(pdisp, (long)hwnd, SWC_EXPLORER, &cookie))) {
                        psw->Revoke(cookie);
                    }
                }
            }
        }
    }
    return hr;
}
