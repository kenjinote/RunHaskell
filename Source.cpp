#pragma comment(lib, "shlwapi")
#pragma comment(lib, "winmm")
#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>
#include "zip.h"
#include "unzip.h"
#include "resource.h"

#define WM_EXITTHREAD (WM_APP + 100)

TCHAR szClassName[] = TEXT("HASKELL");
WNDPROC lpfnOldClassProc;

BOOL DeleteDirectory(LPCTSTR lpPathName)
{
	if (0 == lpPathName) return FALSE;
	TCHAR szDirectoryPathName[MAX_PATH];
	wcsncpy_s(szDirectoryPathName, MAX_PATH, lpPathName, _TRUNCATE);
	if (TEXT('\\') == szDirectoryPathName[wcslen(szDirectoryPathName) - 1])
	{
		szDirectoryPathName[wcslen(szDirectoryPathName) - 1] = TEXT('\0');
	}
	szDirectoryPathName[wcslen(szDirectoryPathName) + 1] = TEXT('\0');
	SHFILEOPSTRUCT fos = { 0 };
	fos.wFunc = FO_DELETE;
	fos.pFrom = szDirectoryPathName;
	fos.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
	return !SHFileOperation(&fos);
}

BOOL CreateTempDirectory(LPTSTR pszDir)
{
	DWORD dwSize = GetTempPath(0, 0);
	if (dwSize == 0 || dwSize>MAX_PATH - 14) return FALSE;
	LPTSTR pTmpPath = (LPTSTR)GlobalAlloc(GPTR, sizeof(TCHAR)*(dwSize + 1));
	GetTempPath(dwSize + 1, pTmpPath);
	dwSize = GetTempFileName(pTmpPath, TEXT(""), 0, pszDir);
	GlobalFree(pTmpPath);
	if (dwSize == 0) return FALSE;
	DeleteFile(pszDir);
	return (CreateDirectory(pszDir, 0) != 0);
}

LPWSTR Open(LPCTSTR lpszFilePath)
{
	const HANDLE hFile = CreateFile(lpszFilePath, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		const int nSize = GetFileSize(hFile, 0);
		LPSTR lpszCode = (LPSTR)GlobalAlloc(0, nSize + 1);
		DWORD dwRead;
		ReadFile(hFile, lpszCode, nSize, &dwRead, 0);
		CloseHandle(hFile);
		lpszCode[dwRead] = 0;
		DWORD len = MultiByteToWideChar(CP_UTF8, 0, lpszCode, -1, 0, 0);
		LPWSTR pwsz = (LPWSTR)GlobalAlloc(0, len * sizeof(WCHAR));
		MultiByteToWideChar(CP_UTF8, 0, lpszCode, -1, pwsz, len);
		GlobalFree(lpszCode);
		return pwsz;
	}
	return 0;
}

LPWSTR OpenAs(HWND hWnd)
{
	static TCHAR szFilePath[MAX_PATH];
	OPENFILENAME of = { 0 };
	TCHAR fname[MAX_PATH];
	TCHAR ftitle[MAX_PATH];
	lstrcpy(fname, lstrlen(szFilePath) ? PathFindFileName(szFilePath) : TEXT("code.hs"));
	ftitle[0] = 0;
	of.lStructSize = sizeof(OPENFILENAME);
	of.hwndOwner = hWnd;
	of.lpstrFilter = TEXT("Haskell ファイル (*.hs)\0*.hs\0すべてのファイル (*.*)\0*.*\0\0");
	of.lpstrFile = fname;
	of.lpstrFileTitle = ftitle;
	of.nMaxFile = MAX_PATH;
	of.nMaxFileTitle = MAX_PATH;
	of.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	of.lpstrDefExt = TEXT("hs");
	of.lpstrTitle = TEXT("開く");
	if (GetOpenFileName(&of))
	{
		lstrcpy(szFilePath, fname);
		return Open(szFilePath);
	}
	return 0;
}

BOOL Save(LPCSTR lpszCode, DWORD dwSize, LPCTSTR lpszFilePath)
{
	const HANDLE hFile = CreateFile(lpszFilePath, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		DWORD d;
		WriteFile(hFile, lpszCode, dwSize, &d, 0);
		CloseHandle(hFile);
		return TRUE;
	}
	return FALSE;
}

BOOL SaveAs(HWND hWnd, LPCSTR lpszCode, DWORD dwSize)
{
	static TCHAR szFilePath[MAX_PATH];
	OPENFILENAME of = { 0 };
	TCHAR fname[MAX_PATH];
	TCHAR ftitle[MAX_PATH];
	lstrcpy(fname, lstrlen(szFilePath) ? PathFindFileName(szFilePath) : TEXT("code.hs"));
	ftitle[0] = 0;
	of.lStructSize = sizeof(OPENFILENAME);
	of.hwndOwner = hWnd;
	of.lpstrFilter = TEXT("Haskell ファイル (*.hs)\0*.hs\0すべてのファイル (*.*)\0*.*\0\0");
	of.lpstrFile = fname;
	of.lpstrFileTitle = ftitle;
	of.nMaxFile = MAX_PATH;
	of.nMaxFileTitle = MAX_PATH;
	of.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	of.lpstrDefExt = TEXT("hs");
	of.lpstrTitle = TEXT("保存");
	if (GetSaveFileName(&of))
	{
		lstrcpy(szFilePath, fname);
		return Save(lpszCode, dwSize, szFilePath);
	}
	return FALSE;
}

void BuildAndRun(LPCSTR lpszCode, DWORD dwSize, LPCTSTR lpszTempPath, HWND hOutputEdit)
{
	// ファイルへ保存
	TCHAR szCodeFilePath[MAX_PATH];
	lstrcpy(szCodeFilePath, lpszTempPath);
	PathAppend(szCodeFilePath, TEXT("code.hs"));
	if (!Save(lpszCode, dwSize, szCodeFilePath)) return;
	SetWindowTextA(hOutputEdit, 0);
	TCHAR szCurrentDirectory[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, szCurrentDirectory);
	SetCurrentDirectory(lpszTempPath);
	//リダイレクト先のファイルを開く
	SECURITY_ATTRIBUTES sa = { 0 };
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE; //TRUEでハンドルを引き継ぐ
	TCHAR szOutputFilePath[MAX_PATH];
	lstrcpy(szOutputFilePath, lpszTempPath);
	PathAppend(szOutputFilePath, TEXT("stdout.txt"));
	HANDLE hFile = CreateFile(szOutputFilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	//標準入出力の指定
	STARTUPINFO si = { 0 };
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = stdin;
	si.hStdOutput = hFile;
	si.hStdError = hFile;
	//コンパイラを起動する
	TCHAR szBuildCommand[1024];
	wsprintf(szBuildCommand, TEXT("%s\\BIN\\GHC.EXE --make %s"), lpszTempPath, szCodeFilePath);
	PROCESS_INFORMATION pi = { 0 };
	const DWORD dwBuildStartTime = timeGetTime();
	if (!CreateProcess(0, szBuildCommand, 0, 0, TRUE, CREATE_NO_WINDOW, 0, 0, &si, &pi))
	{
		const DWORD len = SendMessageA(hOutputEdit, WM_GETTEXTLENGTH, 0, 0);
		SendMessageA(hOutputEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
		SendMessageA(hOutputEdit, EM_REPLACESEL, 0, (LPARAM)"ビルド失敗\r\n\r\n");
	}
	//起動したプロセスの終了を待つ
	WaitForSingleObject(pi.hProcess, INFINITE);
	const DWORD dwBuildTime = timeGetTime() - dwBuildStartTime;
	DWORD dwExidCode = 0;
	GetExitCodeProcess(pi.hProcess, &dwExidCode);
	//ハンドルを閉じる
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	DWORD dwRunTime = -1;
	if (dwExidCode == 0)
	{
		CloseHandle(hFile);
		hFile = CreateFile(szOutputFilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
		si.hStdOutput = hFile;
		si.hStdError = hFile;
		//ビルドされたプログラムを起動する
		TCHAR szRunCommand[1024];
		lstrcpy(szRunCommand, TEXT("code.exe"));
		const DWORD dwRunStartTime = timeGetTime();
		if (!CreateProcess(0, szRunCommand, 0, 0, TRUE, CREATE_NO_WINDOW, 0, 0, &si, &pi))
		{
			const DWORD len = SendMessageA(hOutputEdit, WM_GETTEXTLENGTH, 0, 0);
			SendMessageA(hOutputEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
			SendMessageA(hOutputEdit, EM_REPLACESEL, 0, (LPARAM)"プログラムの起動失敗\r\n\r\n");
		}
		//起動したプロセスの終了を待つ
		WaitForSingleObject(pi.hProcess, INFINITE);
		dwRunTime = timeGetTime() - dwRunStartTime;
		//ハンドルを閉じる
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	SetFilePointer(hFile, 0, 0, FILE_BEGIN);
	const int nFileSize = GetFileSize(hFile, 0);
	LPSTR lpszOutput = (LPSTR)GlobalAlloc(0, nFileSize + 1);
	DWORD d;
	ReadFile(hFile, lpszOutput, nFileSize, &d, 0);
	lpszOutput[d] = 0;
	{
		const DWORD len = SendMessageA(hOutputEdit, WM_GETTEXTLENGTH, 0, 0);
		SendMessageA(hOutputEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
		SendMessageA(hOutputEdit, EM_REPLACESEL, 0, (LPARAM)lpszOutput);
		SendMessageA(hOutputEdit, EM_REPLACESEL, 0, (LPARAM)"\r\n");
	}
	GlobalFree(lpszOutput);
	CloseHandle(hFile);
	DeleteFile(szCodeFilePath);
	DeleteFile(szOutputFilePath);
	SetCurrentDirectory(szCurrentDirectory);
	const DWORD len = SendMessageA(hOutputEdit, WM_GETTEXTLENGTH, 0, 0);
	SendMessageA(hOutputEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
	CHAR szTime[1024];
	wsprintfA(szTime, "ビルド時間: %d msec\r\n実行時間: %d msec\r\n", dwBuildTime, dwRunTime);
	SendMessageA(hOutputEdit, EM_REPLACESEL, 0, (LPARAM)szTime);
}

void BuildAndExport(HWND hWnd, LPCSTR lpszCode, DWORD dwSize, LPCTSTR lpszTempPath, HWND hOutputEdit)
{
	SetWindowTextA(hOutputEdit, 0);
	TCHAR szCurrentDirectory[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, szCurrentDirectory);
	SetCurrentDirectory(lpszTempPath);
	TCHAR szCodeFilePath[MAX_PATH];
	lstrcpy(szCodeFilePath, lpszTempPath);
	PathAppend(szCodeFilePath, TEXT("code.hs"));
	HANDLE hFile = CreateFile(szCodeFilePath, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	DWORD d;
	WriteFile(hFile, lpszCode, dwSize, &d, 0);
	CloseHandle(hFile);
	//リダイレクト先のファイルを開く
	SECURITY_ATTRIBUTES sa = { 0 };
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE; //TRUEでハンドルを引き継ぐ
	TCHAR szOutputFilePath[MAX_PATH];
	lstrcpy(szOutputFilePath, lpszTempPath);
	PathAppend(szOutputFilePath, TEXT("stdout.txt"));
	hFile = CreateFile(szOutputFilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	//標準入出力の指定
	STARTUPINFO si = { 0 };
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = stdin;
	si.hStdOutput = hFile;
	si.hStdError = hFile;
	//コンパイラを起動する
	TCHAR szBuildCommand[1024];
	wsprintf(szBuildCommand, TEXT("%s\\BIN\\GHC.EXE --make %s"), lpszTempPath, szCodeFilePath);
	PROCESS_INFORMATION pi = { 0 };
	const DWORD dwBuildStartTime = timeGetTime();
	if (!CreateProcess(0, szBuildCommand, 0, 0, TRUE, CREATE_NO_WINDOW, 0, 0, &si, &pi))
	{
		const DWORD len = SendMessageA(hOutputEdit, WM_GETTEXTLENGTH, 0, 0);
		SendMessageA(hOutputEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
		SendMessageA(hOutputEdit, EM_REPLACESEL, 0, (LPARAM)"ビルド失敗\r\n\r\n");
	}
	//起動したプロセスの終了を待つ
	WaitForSingleObject(pi.hProcess, INFINITE);
	const DWORD dwBuildTime = timeGetTime() - dwBuildStartTime;
	DWORD dwExidCode = 0;
	GetExitCodeProcess(pi.hProcess, &dwExidCode);
	//ハンドルを閉じる
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	if (dwExidCode == 0)
	{
		//ビルドされたプログラムを起動する
		TCHAR szRunCommand[1024];
		lstrcpy(szRunCommand, lpszTempPath);
		PathAppend(szRunCommand, TEXT("code.exe"));
		TCHAR fname[MAX_PATH] = { 0 };
		TCHAR ftitle[MAX_PATH] = { 0 };
		OPENFILENAME of = { 0 };
		lstrcpy(fname, TEXT("code.exe"));
		of.lStructSize = sizeof(OPENFILENAME);
		of.hwndOwner = hWnd;
		of.lpstrFilter = TEXT("アプリケーション\0*.exe\0\0");
		of.lpstrFile = fname;
		of.lpstrFileTitle = ftitle;
		of.nMaxFile = MAX_PATH;
		of.nMaxFileTitle = MAX_PATH;
		of.Flags = OFN_FILEMUSTEXIST | OFN_OVERWRITEPROMPT;
		of.lpstrDefExt = TEXT("exe");
		of.lpstrTitle = TEXT("名前を付けて保存");
		if (GetSaveFileName(&of))
		{
			MoveFileEx(szRunCommand, fname, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH | MOVEFILE_COPY_ALLOWED);
		}
	}
	SetFilePointer(hFile, 0, 0, FILE_BEGIN);
	const int nFileSize = GetFileSize(hFile, 0);
	LPSTR lpszOutput = (LPSTR)GlobalAlloc(0, nFileSize + 1);
	ReadFile(hFile, lpszOutput, nFileSize, &d, 0);
	lpszOutput[d] = 0;
	{
		const DWORD len = SendMessageA(hOutputEdit, WM_GETTEXTLENGTH, 0, 0);
		SendMessageA(hOutputEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
		SendMessageA(hOutputEdit, EM_REPLACESEL, 0, (LPARAM)lpszOutput);
		SendMessageA(hOutputEdit, EM_REPLACESEL, 0, (LPARAM)"\r\n");
	}
	GlobalFree(lpszOutput);
	CloseHandle(hFile);
	DeleteFile(szCodeFilePath);
	DeleteFile(szOutputFilePath);
	SetCurrentDirectory(szCurrentDirectory);
	const DWORD len = SendMessageA(hOutputEdit, WM_GETTEXTLENGTH, 0, 0);
	SendMessageA(hOutputEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
	CHAR szTime[1024];
	wsprintfA(szTime, "ビルド時間: %d msec\r\n", dwBuildTime);
	SendMessageA(hOutputEdit, EM_REPLACESEL, 0, (LPARAM)szTime);
}

LRESULT CALLBACK MultiLineEditWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_GETDLGCODE) return DLGC_WANTALLKEYS;
	return CallWindowProc(lpfnOldClassProc, hWnd, msg, wParam, lParam);
}

struct DATA
{
	HWND hWnd;
	TCHAR szTempPath[MAX_PATH];
	DWORD dwProgress;
	BOOL bAbort;
};

DWORD WINAPI ThreadExpandModule(LPVOID p)
{
	DATA* pData = (DATA*)p;
	const HMODULE hInst = GetModuleHandle(0);
	const HRSRC hResource = FindResource(hInst, MAKEINTRESOURCE(IDR_ZIP1), TEXT("ZIP"));
	if (!hResource) return FALSE;
	const DWORD imageSize = SizeofResource(hInst, hResource);
	if (!imageSize) return FALSE;
	const void* pResourceData = LockResource(LoadResource(hInst, hResource));
	if (!pResourceData) return FALSE;
	TCHAR szCurrentDirectory[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, szCurrentDirectory);
	SetCurrentDirectory(pData->szTempPath);
	HZIP hz = OpenZip((LPBYTE)pResourceData, imageSize, 0);
	SetUnzipBaseDir(hz, pData->szTempPath);
	ZIPENTRY ze;
	GetZipItem(hz, -1, &ze);
	const int numitems = ze.index;
	for (int zi = 0; zi < numitems && !pData->bAbort; ++zi)
	{
		pData->dwProgress = (DWORD)(100.0 * (double)zi / (double)numitems);
		GetZipItem(hz, zi, &ze);
		if (ze.attr & FILE_ATTRIBUTE_DIRECTORY)
		{
			CreateDirectory(ze.name, 0);
		}
		else if (ze.attr & FILE_ATTRIBUTE_ARCHIVE)
		{
			UnzipItem(hz, zi, ze.name);
			char *ibuf = new char[ze.unc_size];
			UnzipItem(hz, zi, ibuf, ze.unc_size);
			const HANDLE hFile = CreateFile(ze.name, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
			DWORD d;
			WriteFile(hFile, ibuf, ze.unc_size, &d, 0);
			CloseHandle(hFile);
			delete[] ibuf;
		}
	}
	CloseZip(hz);
	SetCurrentDirectory(szCurrentDirectory);
	PostMessage(pData->hWnd, WM_EXITTHREAD, 0, (LPARAM)p);
	ExitThread(0);
}

int CALLBACK DialogProc(HWND hDlg, unsigned msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HWND hInputEdit, hOutputEdit;
	static TCHAR szTempDirectoryPath[MAX_PATH];
	static HFONT hFont;
	static DATA* pData;
	static HANDLE hThread;
	static DWORD dwParam;
	switch (msg)
	{
	case WM_CREATE:
		if (!CreateTempDirectory(szTempDirectoryPath)) return -1;
		pData = (DATA*)GlobalAlloc(0, sizeof DATA);
		pData->hWnd = hWnd;
		pData->bAbort = 0;
		pData->dwProgress = 0;
		lstrcpy(pData->szTempPath, szTempDirectoryPath);
		hThread = CreateThread(0, 0, ThreadExpandModule, (LPVOID)pData, 0, &dwParam);
		hFont = CreateFont(20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, TEXT("Consolas"));
		hInputEdit = CreateWindow(TEXT("EDIT"), TEXT("main = putStrLn \"Hello, World!\"\r\n"), WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | WS_TABSTOP | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_WANTRETURN, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessage(hInputEdit, EM_LIMITTEXT, 0, 0);
		{
			const int tab = 16;
			SendMessage(hInputEdit, EM_SETTABSTOPS, 1, (LPARAM)&tab);
		}
		lpfnOldClassProc = (WNDPROC)SetWindowLong(hInputEdit, GWL_WNDPROC, (LONG)MultiLineEditWndProc);
		hOutputEdit = CreateWindow(TEXT("EDIT"), TEXT("ビルドの環境を構築しています..."), WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | WS_TABSTOP | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_READONLY, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessage(hOutputEdit, EM_LIMITTEXT, 0, 0);
		SendMessage(hInputEdit, WM_SETFONT, (WPARAM)hFont, 0);
		SendMessage(hOutputEdit, WM_SETFONT, (WPARAM)hFont, 0);
		break;
	case WM_EXITTHREAD:
	{
		WaitForSingleObject(hThread, INFINITE);
		CloseHandle(hThread);
		hThread = 0;
		const BOOL bExit = pData->bAbort;
		GlobalFree(pData);
		pData = 0;
		SetWindowText(hOutputEdit, TEXT("ビルドの環境が整いました。"));
		if (bExit) PostMessage(hWnd, WM_CLOSE, 0, 0);
	}
	break;
	case WM_SETFOCUS:
		SetFocus(hInputEdit);
		break;
	case WM_SIZE:
		MoveWindow(hInputEdit, 0, 0, LOWORD(lParam) / 2, HIWORD(lParam), 1);
		MoveWindow(hOutputEdit, LOWORD(lParam) / 2, 0, LOWORD(lParam) / 2, HIWORD(lParam), 1);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_BUILD:
			if (hThread)
			{
				SetWindowText(hOutputEdit, TEXT("ビルドの準備がまだできていません。"));
			}
			else
			{
				const int nSize = GetWindowTextLengthW(hInputEdit);
				LPWSTR lpszCode = (LPWSTR)GlobalAlloc(0, sizeof(WCHAR)*(nSize + 1));
				GetWindowTextW(hInputEdit, lpszCode, nSize + 1);
				const DWORD len = WideCharToMultiByte(CP_UTF8, 0, lpszCode, -1, 0, 0, 0, 0);
				LPSTR psz = (LPSTR)GlobalAlloc(GPTR, sizeof(CHAR)*len);
				WideCharToMultiByte(CP_UTF8, 0, lpszCode, -1, psz, len, 0, 0);
				GlobalFree(lpszCode);
				BuildAndRun(psz, len - 1, szTempDirectoryPath, hOutputEdit);
				GlobalFree(psz);
			}
			break;
		case ID_INPUT:
			SetFocus(hInputEdit);
			break;
		case ID_OUTPUT:
			SetFocus(hOutputEdit);
			break;
		case ID_ALLSELECT:
			SendMessage(GetFocus(), EM_SETSEL, 0, -1);
			break;
		case ID_EXE_EXPORT:
			if (hThread)
			{
				SetWindowText(hOutputEdit, TEXT("ビルドの準備がまだできていません。"));
			}
			else
			{
				const int nSize = GetWindowTextLengthW(hInputEdit);
				LPWSTR lpszCode = (LPWSTR)GlobalAlloc(0, sizeof(WCHAR)*(nSize + 1));
				GetWindowTextW(hInputEdit, lpszCode, nSize + 1);
				const DWORD len = WideCharToMultiByte(CP_UTF8, 0, lpszCode, -1, 0, 0, 0, 0);
				LPSTR psz = (LPSTR)GlobalAlloc(GPTR, sizeof(CHAR)*len);
				WideCharToMultiByte(CP_UTF8, 0, lpszCode, -1, psz, len, 0, 0);
				GlobalFree(lpszCode);
				BuildAndExport(hWnd, psz, len - 1, szTempDirectoryPath, hOutputEdit);
				GlobalFree(psz);
			}
			break;
		case ID_OPEN:
		{
			LPWSTR lpszCode = OpenAs(hWnd);
			if (lpszCode)
			{
				SetWindowText(hInputEdit, lpszCode);
				GlobalFree(lpszCode);
			}
		}
		break;
		case ID_SAVEAS:
		{
			const int nSize = GetWindowTextLengthW(hInputEdit);
			LPWSTR lpszCode = (LPWSTR)GlobalAlloc(0, sizeof(WCHAR)*(nSize + 1));
			GetWindowTextW(hInputEdit, lpszCode, nSize + 1);
			const DWORD len = WideCharToMultiByte(CP_UTF8, 0, lpszCode, -1, 0, 0, 0, 0);
			LPSTR psz = (LPSTR)GlobalAlloc(GPTR, sizeof(CHAR)*len);
			WideCharToMultiByte(CP_UTF8, 0, lpszCode, -1, psz, len, 0, 0);
			GlobalFree(lpszCode);
			SaveAs(hWnd, psz, len - 1);
			GlobalFree(psz);
		}
		break;
		}
		break;
	case WM_CLOSE:
		if (hThread)
		{
			pData->bAbort = TRUE;
		}
		else
		{
			DestroyWindow(hWnd);
		}
		break;
	case WM_DESTROY:
		DeleteDirectory(szTempDirectoryPath);
		PostQuitMessage(0);
		break;
	default:
		return DefDlgProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR pCmdLine, int nCmdShow)
{
	MSG msg;
	const WNDCLASS wndclass = {
		0,
		WndProc,
		0,
		DLGWINDOWEXTRA,
		hInstance,
		LoadIcon(hInstance,(LPCTSTR)IDI_ICON1),
		0,
		0,
		0,
		szClassName
	};
	RegisterClass(&wndclass);
	const HWND hWnd = CreateWindow(
		szClassName,
		TEXT("Haskell"),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT,
		0,
		CW_USEDEFAULT,
		0,
		0,
		0,
		hInstance,
		0
	);
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);
	ACCEL Accel[] = {
		{ FVIRTKEY, VK_F5, ID_BUILD },
		{ FVIRTKEY | FCONTROL, VK_RETURN, ID_BUILD },
		{ FVIRTKEY | FCONTROL, 'I', ID_INPUT },
		{ FVIRTKEY | FCONTROL, 'O', ID_OPEN },
		{ FVIRTKEY | FCONTROL, 'S', ID_SAVEAS },
		{ FVIRTKEY | FCONTROL, 'A', ID_ALLSELECT },
		{ FVIRTKEY | FCONTROL, 'E', ID_EXE_EXPORT },
	};
	const HACCEL hAccel = CreateAcceleratorTable(Accel, sizeof(Accel) / sizeof(ACCEL));
	while (GetMessage(&msg, 0, 0, 0))
	{
		if (!TranslateAccelerator(hWnd, hAccel, &msg) && !IsDialogMessage(hWnd, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	DestroyAcceleratorTable(hAccel);
	return msg.wParam;
}