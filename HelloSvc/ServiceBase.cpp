#include "stdafx.h"

_NT_BEGIN

#include "ServiceBase.h"

CServiceBase::CServiceBase()
{
	InitializeCriticalSection(this);
}

CServiceBase::~CServiceBase()
{
	DeleteCriticalSection(this);
}

DWORD WINAPI CServiceBase::HandlerEx(
									 DWORD    dwControl,
									 DWORD    dwEventType,
									 PVOID   lpEventData,
									 PVOID   lpContext
									 )
{
	DbgPrint("HandlerEx(%x %x %p %p)\n", dwControl, dwEventType, lpEventData, lpContext);
	return static_cast<CServiceBase*>(lpContext)->PreHandler(dwControl, dwEventType, lpEventData);
}

DWORD CServiceBase::PreHandler(DWORD dwControl)
{
	switch(dwControl)
	{
	case SERVICE_CONTROL_CONTINUE:

		if (SERVICE_PAUSED != dwCurrentState || 
			!BeginChangeState(SERVICE_ACCEPT_PAUSE_CONTINUE, SERVICE_CONTINUE_PENDING, SERVICE_RUNNING))
		{
			return ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
		}
		break;

	case SERVICE_CONTROL_PAUSE:

		if (SERVICE_RUNNING != dwCurrentState ||
			!BeginChangeState(SERVICE_ACCEPT_PAUSE_CONTINUE, SERVICE_PAUSE_PENDING, SERVICE_PAUSED))
		{
			return ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
		}
		break;

	case SERVICE_CONTROL_STOP:

		if (!BeginChangeState(SERVICE_ACCEPT_STOP, SERVICE_STOP_PENDING, SERVICE_STOPPED))
		{
			return ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
		}
		break;
	}

	return SetServiceStatus(m_ssh, this) ? NOERROR : GetLastError();
}

DWORD CServiceBase::PreHandler(DWORD dwControl, DWORD dwEventType, PVOID lpEventData)
{
	switch (dwControl)
	{
	case SERVICE_CONTROL_INTERROGATE:
		return NOERROR;
	case SERVICE_CONTROL_CONTINUE:
	case SERVICE_CONTROL_PAUSE:
	case SERVICE_CONTROL_STOP:
		LockState();
		ULONG err = PreHandler(dwControl);
		UnlockState();
		if (err)
		{
			return err;
		}
	}

	return Handler(dwControl, dwEventType, lpEventData);
}

ULONG CServiceBase::SetState(DWORD WaitHint)
{
	ULONG err = ERROR_INVALID_STATE;

	LockState();

	if (dwCheckPoint)
	{
		dwCheckPoint++;
		dwWaitHint = WaitHint;
		err = SetServiceStatus(m_ssh, this) ? NOERROR : GetLastError();
	}

	UnlockState();

	return err;
}

ULONG CServiceBase::SetState(DWORD NewState, DWORD ControlsAccepted, DWORD Win32ExitCode)
{
	LockState();

	if (dwCurrentState != NewState)
	{
		dwCurrentState = NewState;
		dwControlsAccepted = ControlsAccepted;
		dwWin32ExitCode = Win32ExitCode;
		dwCheckPoint = 0;
		dwWaitHint = 0;

		Win32ExitCode = SetServiceStatus(m_ssh, this) ? NOERROR : GetLastError();
	}

	UnlockState();

	return Win32ExitCode;
}

BOOL CServiceBase::BeginChangeState(DWORD MustAccept, DWORD dwNewState, DWORD dwTargetState)
{
	if (dwCheckPoint || !(dwControlsAccepted & MustAccept)) return FALSE;

	dwCurrentState = dwNewState;
	dwCheckPoint = 1;
	dwWaitHint = 2000;
	dwControlsAccepted &= ~(SERVICE_CONTROL_CONTINUE|SERVICE_CONTROL_PAUSE|SERVICE_CONTROL_STOP);

	m_dwTargetState = dwTargetState;

	return TRUE;
}

void CServiceBase::ServiceMain(PWSTR ServiceName)
{
	m_dwTargetState = SERVICE_RUNNING;

	dwServiceType = SERVICE_WIN32_SHARE_PROCESS;
	dwCurrentState = SERVICE_START_PENDING;
	dwControlsAccepted = SERVICE_ACCEPT_PARAMCHANGE;
	dwWin32ExitCode = NOERROR;
	dwServiceSpecificExitCode = 0;
	dwCheckPoint = 1;
	dwWaitHint = 2000;

	if (m_ssh = RegisterServiceCtrlHandlerEx(ServiceName, HandlerEx, this))
	{
		SetState(SERVICE_STOPPED, 0, Run());
	}
}

_NT_END