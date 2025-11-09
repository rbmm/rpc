#pragma once

ULONG InitRpcServer();
ULONG OpenPolicy();
void ClosePolicy();

inline ULONG StartListen()
{
	return RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT , TRUE);
}
