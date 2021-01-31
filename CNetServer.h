#pragma once
#pragma comment(lib, "ws2_32.lib")
#include<WinSock2.h>
#include<WS2tcpip.h>
#include <Windows.h>
#include <iostream>
#include <process.h>
#include <unordered_map>
#include <set>
#include <stack>
#include <list>

#include "CRingBuffer\CRingBuffer.h"
#include "CSerializationBuffer\CSerializationBuffer.h"
#include "CLockFreeQueue\LockFreeQueue.h"
#include "CLockFreeStack\LockFreeStack.h"

#define MAXWSABUF 200

class CNetServer
{
public:
	CNetServer();
	//오픈 ip, 포트, 워커스레트 수, 네이글 옵션, 최대접속자 수
	bool Start(const wchar_t* openIP, short port, int workerThreadRunNum, int workerThreadCreateNum, bool nagleObtion, int maxClientNum);
	
	bool SendPacket(UINT64, CSerializationBuffer*);
	
	bool Disconnect(UINT64);
	
	INT GetClientCount();
	
	UINT64 GetAcceptCount();
	
	VOID Stop();

	//Accept후 접속처리 완료 후 호출
	virtual VOID OnClientJoin(UINT64) = 0;

	//Release후 호출
	virtual VOID OnClientLeave(UINT64) = 0;

	//accept직후, return false, return true;
	//false 시 클라이언트 거부, true 시 접속 허용
	virtual bool OnConnectionRequest(PWCHAR, short) = 0;

	//패킷 수신 완료 후
	virtual VOID OnRecv(UINT64, CSerializationBuffer*) = 0;

	//패킷 송신 완료 후
	//virtual VOID OnSend() = 0;
	//
	//워커스레드 GQCS 바로 하단에서 호출
	//virtual VOID OnWorkerThreadBegin() = 0;
	//
	//워커스레드 1루프 종료 후
	//virtual VOID OnWorkerThreadEnd() = 0;

	virtual VOID OnError() = 0;

public:
	SOCKET mListenSocket;
	UINT64 acceptCount = 0;
	UINT64 acceptTPS = 0;
	UINT64 recvTPS = 0;
	UINT64 sendTPS = 0;
	UINT64 mCurrentClientCount;
	UINT64 mClientID;
	INT mMaxClientNum;
	INT mWorkerThreadRunNum;
	INT mWorkerThreadCreateNum;
	SHORT mServerPort;
	HANDLE mIocpHandle;
	HANDLE* mThreadArr;

	struct stIO_RELEASE
	{
		SHORT IOCount;
		SHORT releaseFlag;
	};
	__declspec(align(64))struct stSESSION
	{
		UINT64 sessionID;
		SOCKET socket;
		SOCKET socketForRelease;
		INT sessionArrayIndex;


		__declspec(align(64))OVERLAPPED recvOverlapped;
		OVERLAPPED sendOverlapped;
		bool sendFlag;
		INT sendCount;
		stIO_RELEASE ioRelease;
		
		CRingBuffer recvQ;
		LockFreeQueue<CSerializationBuffer*> sendQ;
		CSerializationBuffer* sendPacketPtrBuf[MAXWSABUF];
	};
#pragma pack(1)
	struct stHEADER
	{
		UCHAR code;
		USHORT length;
		UCHAR randKey;
		UCHAR checkSum;
	};
#pragma pack()
	LockFreeStack<INT> mSessionArrayIndexStack;
	stSESSION* mSessionArray;

	static unsigned WINAPI StaticWorkerThread(PVOID p);
	VOID WorkerThread();

	VOID RecvProc(SHORT, stSESSION*);

	VOID SendProc(stSESSION*);

	static unsigned WINAPI StaticAcceptThread(PVOID p);
	VOID AcceptThread();

	VOID RecvPost(stSESSION*);
	VOID SendPost(stSESSION*);

	stSESSION* FindSession(UINT64);
	UINT64 MakeSessionID(USHORT, UINT64);

	bool ReleaseSession(stSESSION*);

	bool AcquireSession(UINT64, stSESSION*);

	void LeaveSession(stSESSION*);
};
