#pragma once

class DECLSPEC_NOVTABLE CServiceBase : public SERVICE_STATUS, CRITICAL_SECTION
{
private:

	BOOL BeginChangeState(DWORD MustAccept, DWORD dwNewState, DWORD dwTargetState);

protected:
	static DWORD WINAPI HandlerEx(
		DWORD    dwControl,
		DWORD    dwEventType,
		PVOID   lpEventData,
		PVOID   lpContext
		);

	DWORD PreHandler(DWORD dwControl);

	DWORD PreHandler(DWORD dwControl, DWORD dwEventType, PVOID lpEventData);

	SERVICE_STATUS_HANDLE m_ssh;

protected:
	DWORD m_dwTargetState;

	virtual DWORD Run() = 0;

	virtual DWORD Handler(
		DWORD    dwControl,
		DWORD    dwEventType,
		PVOID   lpEventData
		) = 0;

	ULONG SetState(DWORD NewState, DWORD ControlsAccepted, DWORD Win32ExitCode = NOERROR);
	
	ULONG SetState(DWORD WaitHint);

	DWORD GetState() { return dwCurrentState; }

	void LockState()
	{
		EnterCriticalSection(this);
	}

	void UnlockState()
	{
		LeaveCriticalSection(this);
	}

public:

	void ServiceMain(PWSTR ServiceName);

	CServiceBase();

	virtual ~CServiceBase();
};
