#include "stdafx.h"
#include "hello_h.h"

volatile UCHAR guz;
_NT_BEGIN

#include "RpcSvc.h"
#include "ServiceBase.h"

//////////////////////////////////////////////////////////////////////////

class CService : public CServiceBase
{
	HANDLE m_hEvent;
	BOOLEAN m_bStop, m_bListen;

	virtual DWORD Run();

	virtual DWORD Handler(
		DWORD    dwControl,
		DWORD    dwEventType,
		PVOID   lpEventData
		);

public:
	CService() : m_hEvent(0), m_bStop(FALSE) { }
	
	~CService()
	{
		if (m_hEvent)
		{
			CloseHandle(m_hEvent);
		}
	}
};

DWORD CService::Run()
{
	ULONG dwError = (m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL)) ? NOERROR : GetLastError();

	if (dwError == NOERROR && ((dwError = OpenPolicy()) == NOERROR))
	{
		if ((dwError = InitRpcServer()) == RPC_S_OK)
		{
			do 
			{
				if (dwError = StartListen())
				{
					break;
				}

				m_bListen = TRUE;

				if (dwError = SetState(SERVICE_RUNNING, SERVICE_ACCEPT_PAUSE_CONTINUE|SERVICE_ACCEPT_STOP, 0))
				{
					m_bStop = TRUE;
					RpcMgmtStopServerListening(0);
				}

				RpcMgmtWaitServerListen();

				m_bListen = FALSE;
				
				if (m_bStop)
				{
					break;
				}

				if (dwError = SetState(SERVICE_PAUSED, SERVICE_ACCEPT_PAUSE_CONTINUE|SERVICE_ACCEPT_STOP, 0))
				{
					break;
				}

				if (WaitForSingleObject(m_hEvent, INFINITE) == WAIT_FAILED)
				{
					dwError = GetLastError();
					break;
				}

			} while (!m_bStop);
		}

		ClosePolicy();
	}

	return dwError;
}

DWORD CService::Handler( DWORD dwControl, DWORD , PVOID  )
{
	switch (dwControl)
	{
	case SERVICE_CONTROL_STOP:
		m_bStop = TRUE;
		return m_bListen ? RpcMgmtStopServerListening(0) : BOOL_TO_ERROR(SetEvent(m_hEvent));

	case SERVICE_CONTROL_PAUSE:
		return RpcMgmtStopServerListening(0);

	case SERVICE_CONTROL_CONTINUE:
		return BOOL_TO_ERROR(SetEvent(m_hEvent));
	}
	return ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
}

//////////////////////////////////////////////////////////////////////////

void NTAPI ServiceMain(DWORD argc, PWSTR argv[])
{
	if (argc)
	{
		CService o;
		o.ServiceMain(argv[0]);
	}
}

ULONG ShowUi();

void NTAPI ServiceEntry(void*)
{
	if (wcschr(GetCommandLineW(), '\n'))
	{
		const static SERVICE_TABLE_ENTRY ste[] = { { const_cast<PWSTR>(L"HelloSvc"), ServiceMain }, {} };
		StartServiceCtrlDispatcher(ste);
	}
	else 
	{
		ULONG SessionId;
		if (ProcessIdToSessionId(GetCurrentProcessId(), &SessionId) && 
			WTSGetActiveConsoleSessionId() == SessionId)
		{
			ShowUi();
		}
	}
	ExitProcess(0);
}

_NT_END

