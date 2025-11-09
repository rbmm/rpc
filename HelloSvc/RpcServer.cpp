#include "stdafx.h"

#include "hello_h.h"

extern volatile UCHAR guz;

_NT_BEGIN

#include <Ntlsa.h>
#undef _NTDDK_
#include <sddl.h>

#include "RpcSvc.h"

LSA_HANDLE g_PolicyHandle = 0;

ULONG OpenPolicy()
{
	static LSA_OBJECT_ATTRIBUTES ObjectAttributes = { sizeof(ObjectAttributes) };
	NTSTATUS status = LsaOpenPolicy(0, &ObjectAttributes, POLICY_LOOKUP_NAMES, &g_PolicyHandle);
	return 0 > status ? LsaNtStatusToWinError(status) : NOERROR;
}

void ClosePolicy()
{
	if (g_PolicyHandle) LsaClose(g_PolicyHandle);
}

/************************************************************************/
/* 
this is an excess function because we set Security Descriptor on ALPC port.
all non-admins will got access denied already on connect to our port stage
just for demo only
*/
/************************************************************************/
RPC_STATUS CALLBACK IfCallback(RPC_IF_HANDLE , void* Context)
{
	PSID AdministratorsGroup = alloca(SECURITY_SID_SIZE(2));
	static SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	InitializeSid(AdministratorsGroup, &NtAuthority, 2);
	*GetSidSubAuthority(AdministratorsGroup, 0) = SECURITY_BUILTIN_DOMAIN_RID;
	*GetSidSubAuthority(AdministratorsGroup, 1) = DOMAIN_ALIAS_RID_ADMINS;

	RPC_STATUS status = RpcImpersonateClient(Context);

	if (RPC_S_OK == status)
	{
		BOOL IsMember;

		status = BOOL_TO_ERROR(CheckTokenMembership(0, AdministratorsGroup, &IsMember));

		if (!status && !IsMember)
		{
			status = RPC_S_ACCESS_DENIED;
		}

		if (RpcRevertToSelf() != RPC_S_OK) __debugbreak();
	}

	return status;
}

ULONG InitRpcServer()
{
	ULONG dwError = RpcServerRegisterIfEx(hello_v1_0_s_ifspec,
		NULL, NULL, 0/*RPC_IF_SEC_NO_CACHE*/, RPC_C_LISTEN_MAX_CALLS_DEFAULT, IfCallback);

	if (dwError == RPC_S_OK)
	{
		PSECURITY_DESCRIPTOR SecurityDescriptor;

		// generic all for built-in admins

		dwError = BOOL_TO_ERROR(ConvertStringSecurityDescriptorToSecurityDescriptorW(
			L"D:P(A;;GA;;;BA)", SDDL_REVISION, &SecurityDescriptor, 0));

		if (dwError == ERROR_SUCCESS)
		{
			// create \RPC Control\{C25682E7-183B-47e0-B2A8-3EEF003DD7DC}

			dwError = RpcServerUseProtseqEpW(
				(RPC_WSTR)L"ncalrpc",
				RPC_C_PROTSEQ_MAX_REQS_DEFAULT, 
				(RPC_WSTR)L"{C25682E7-183B-47e0-B2A8-3EEF003DD7DC}", 
				SecurityDescriptor);

			LocalFree(SecurityDescriptor);

			return dwError;
		}
	}

	return dwError;
}


_NT_END

using namespace NT;

unsigned long HelloProc( 
						/* [in] */ handle_t IDL_handle,
						/* [string][in] */ wchar_t *pszRequest,
						/* [string][out] */ wchar_t **ppszResponce)
{
	//MessageBoxW(0, pszRequest, 0, 0);//$$$

	*ppszResponce = 0;

	HANDLE hToken = 0;

	ULONG dwError = RpcImpersonateClient(IDL_handle);

	if (RPC_S_OK == dwError)
	{
		dwError = BOOL_TO_ERROR(OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &hToken));

		if (RpcRevertToSelf() != RPC_S_OK) __debugbreak();
	}

	if (!dwError)
	{
		union {
			::PTOKEN_USER TokenUser;
			PVOID buf;
		};
		PVOID stack = alloca(guz);
		ULONG cb = 0, rcb = sizeof(::TOKEN_USER) + SECURITY_SID_SIZE(2 + SECURITY_NT_NON_UNIQUE_SUB_AUTH_COUNT);

		do 
		{
			if (cb < rcb)
			{
				cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);
			}

			dwError = BOOL_TO_ERROR(GetTokenInformation(hToken, ::TokenUser, buf, cb, &rcb));

		} while (dwError == ERROR_INSUFFICIENT_BUFFER);

		CloseHandle(hToken);

		if (!dwError)
		{
			PLSA_REFERENCED_DOMAIN_LIST ReferencedDomains;
			PLSA_TRANSLATED_NAME Names;

			NTSTATUS status = LsaLookupSids(g_PolicyHandle, 1, &TokenUser->User.Sid, &ReferencedDomains, &Names);

			if (0 <= status)
			{
				wchar_t* pszResponce = 0;
				int len = 0;

				while (0 < (len = _snwprintf(pszResponce, len, L"Hello:\n%wZ\\%wZ\r\nYour request:\n%s\n",
					&ReferencedDomains->Domains[Names->DomainIndex].Name, &Names->Name, pszRequest)))
				{
					if (pszResponce)
					{
						*ppszResponce = pszResponce, pszResponce = 0;
						break;
					}
					if (pszResponce = (wchar_t*)MIDL_user_allocate(++len * sizeof(wchar_t)))
					{
					}
					else
					{
						dwError = ERROR_OUTOFMEMORY;
						break;
					}
				}

				if (pszResponce)
				{
					MIDL_user_free(pszResponce);
					dwError = ERROR_GEN_FAILURE;
				}

				LsaFreeMemory(Names);
				LsaFreeMemory(ReferencedDomains);
			}
			else
			{
				dwError = LsaNtStatusToWinError(status);
			}
		}
	}

	return dwError;
}

void* __RPC_USER MIDL_user_allocate( _In_ size_t size)
{
	return LocalAlloc(0, size);
}

void __RPC_USER MIDL_user_free( _Pre_maybenull_ _Post_invalid_ void  * pv)
{
	LocalFree(pv);
}


