#include "stdafx.h"
#include "resource.h"

extern volatile UCHAR guz;

_NT_BEGIN

#include "RpcSvc.h"

void ShowNTStatus(HWND hwnd, NTSTATUS status, PCWSTR caption)
{
	PWSTR sz;
	if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_HMODULE, 
		GetModuleHandleW(L"ntdll"), status, 0, (PWSTR)&sz, 0, 0))
	{
		UINT type;
		switch ((ULONG)status >> 30)
		{
		case 0:
			type = MB_OK;
			break;
		case 1:
			type = MB_OK|MB_ICONINFORMATION;
			break;
		case 2:
			type = MB_OK|MB_ICONWARNING;
			break;
		case 3:
			type = MB_OK|MB_ICONHAND;
			break;
		default:__assume(false);
		}
		MessageBox(hwnd, sz, caption, type);
		LocalFree(sz);
	}
}

void ShowError(HWND hwnd, ULONG dwError, PCWSTR caption)
{
	NTSTATUS status = RtlGetLastNtStatus();

	if (RtlNtStatusToDosError(status) == dwError)
	{
		ShowNTStatus(hwnd, status, caption);
	}
	else
	{
		PWSTR sz;
		if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM, 
			0, dwError, 0, (PWSTR)&sz, 0, 0))
		{
			MessageBox(hwnd, sz, caption, MB_ICONWARNING);
			LocalFree(sz);
		}
	}
}

CONST PCWSTR ServiceName = L"HelloSvc";

ULONG UnInstall()
{
	ULONG dwError = NOERROR;

	if (SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, 0 ))
	{
		if (SC_HANDLE hService = OpenServiceW(hSCManager, ServiceName, DELETE))
		{
			dwError = BOOL_TO_ERROR(DeleteService(hService));
			CloseServiceHandle(hService);
		}
		else
		{
			dwError = GetLastError();
		}
		CloseServiceHandle(hSCManager);
	}
	else
	{
		dwError = GetLastError();
	}

	return dwError;
}

ULONG InstallService()
{
	PVOID stack = alloca(guz);
	
	union {
		PVOID buf;
		PWSTR lpFilename;
	};

	ULONG cch, nSize;
	do 
	{
		nSize = RtlPointerToOffset(buf = alloca(0x20), stack) / sizeof(WCHAR);

		cch = GetModuleFileNameW(0, lpFilename + 1, nSize - 3);

	} while (cch == nSize - 3);
	
	if (!cch)
	{
		return GetLastError();
	}
		
	*lpFilename = '\"', lpFilename[cch + 1] = '\"', lpFilename[cch + 2] = '\n', lpFilename[cch + 3] = 0;

	ULONG dwError = NOERROR;

	if (SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE ))
	{
		if (SC_HANDLE hService = CreateServiceW(hSCManager, ServiceName, L"Demo Rpc Service", 
			SERVICE_CHANGE_CONFIG, 
			SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
			lpFilename, NULL, NULL, NULL, NULL, NULL))
		{
			static SERVICE_DESCRIPTION sd = { const_cast<PWSTR>(L"Hello Task Demo") };
			
			// not critical if fail. optional
			ChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &sd);

			CloseServiceHandle(hService);
		}
		else
		{
			dwError = GetLastError();
		}
		CloseServiceHandle(hSCManager);
	}
	else
	{
		dwError = GetLastError();
	}

	return dwError;
}

enum { WM_RPC_STOPED = WM_APP };

DWORD WINAPI WaitRpcStop(PVOID hwndDlg)
{
	RpcMgmtWaitServerListen();
	PostMessageW((HWND)hwndDlg, WM_RPC_STOPED, 0, 0);
	return 0;
}

struct CDlg 
{
	BOOLEAN m_bServerListen, m_bWaitExit;

	CDlg() : m_bServerListen(FALSE), m_bWaitExit(FALSE)
	{
	}

	static INT_PTR CALLBACK _DlgProc(HWND hwndDlg, UINT uMgs, WPARAM wParam, LPARAM lParam)
	{
		CDlg* p = 0;

		if (uMgs == WM_INITDIALOG)
		{
			SetWindowLongPtr(hwndDlg, DWLP_USER, lParam);
			p = (CDlg*)lParam;
		}
		else
		{
			p = (CDlg*)GetWindowLongPtr(hwndDlg, DWLP_USER);
		}

		return p ? p->DlgProc(hwndDlg, uMgs, wParam, lParam) : 0;
	}

	INT_PTR DlgProc(HWND hwndDlg, UINT uMgs, WPARAM wParam, LPARAM lParam);
};

INT_PTR CDlg::DlgProc(HWND hwndDlg, UINT uMgs, WPARAM wParam, LPARAM lParam)
{
	ULONG dwError;

	switch (uMgs)
	{
	case WM_DESTROY:
		ClosePolicy();
		break;

	case WM_RPC_STOPED:
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON4), FALSE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON3), TRUE);
		m_bServerListen = FALSE;
		if (m_bWaitExit)
		{
			EndDialog(hwndDlg, 0);
		}
		break;

	case WM_COMMAND:
		switch (wParam)
		{
		case IDC_BUTTON5:
			if ((dwError = OpenPolicy()) || (dwError = InitRpcServer()))
			{
				ShowError(hwndDlg, dwError, L"InitRpcServer");
			}
			else
			{
				EnableWindow((HWND)lParam, FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON3), TRUE);
			}
			break;
		case IDC_BUTTON3:
			if (dwError = StartListen())
			{
				ShowError(hwndDlg, dwError, L"StartListen");
			}
			else
			{
				m_bServerListen = TRUE;
				EnableWindow((HWND)lParam, FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON4), TRUE);
			}
			break;


		case IDCANCEL:
			if (m_bServerListen)
			{
				if (MessageBoxW(hwndDlg, L"Stop Listen ?", L"Server Linstening", MB_ICONWARNING|MB_OKCANCEL) == IDOK)
				{
					m_bWaitExit = TRUE;
		case IDC_BUTTON4:
			if (dwError = RpcMgmtStopServerListening(0))
			{
				ShowError(hwndDlg, dwError, L"StopServerListening");
			}
			else
			{
				EnableWindow((HWND)lParam, FALSE);
				QueueUserWorkItem(WaitRpcStop, hwndDlg, 0);
			}
				}
				break;
			}
			EndDialog(hwndDlg, 0);
			break;

		case IDC_BUTTON1:
			ShowError(hwndDlg, InstallService(), L"InstallService");
			break;
		case IDC_BUTTON2:
			ShowError(hwndDlg, UnInstall(), L"UnInstallService");
			break;
		}
		break;
	}
	return 0;
}

ULONG ShowUi()
{
	LoadLibraryW(L"comctl32");
	CDlg dlg;
	DialogBoxParamW((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG1), HWND_DESKTOP, CDlg::_DlgProc, (LPARAM)&dlg);
	return 0;
}

_NT_END;