#include "CNetServer.h"
unsigned WINAPI CNetServer::StaticWorkerThread(PVOID p)
{
	CNetServer* pObj = (CNetServer*)p;

	pObj->WorkerThread();

	return 0;
}

unsigned WINAPI CNetServer::StaticAcceptThread(PVOID p)
{
	CNetServer* pObj = (CNetServer*)p;
	
	pObj->AcceptThread();

	return 0;
}

VOID CNetServer::WorkerThread()
{
	for (;;)
	{
		stSESSION* pSession = nullptr;
		OVERLAPPED* pOverlapped = nullptr;
		DWORD transferredBytes = 0;

		GetQueuedCompletionStatus(mIocpHandle, &transferredBytes, (PULONG_PTR)&pSession, &pOverlapped, INFINITE);

		//��Ŀ������ ����, ����Ʈť ȣ�� �� & ��ť�����ø��� �Լ� ���� ��
		if (pOverlapped == nullptr)
		{
			PostQueuedCompletionStatus(mIocpHandle, NULL, NULL, NULL);
			break;
		}

		//CancelIo�� ���� IO�� ���� ����Ǿ��� ���
		if (transferredBytes == 0 || pOverlapped->Internal == ERROR_OPERATION_ABORTED)
		{
			goto IOCOUNT_DEQ;
		}

		if (pOverlapped == &pSession->recvOverlapped)
		{
			RecvProc(transferredBytes, pSession);
		}
		else if (pOverlapped == &pSession->sendOverlapped)
		{
			SendProc(pSession);
		}

	IOCOUNT_DEQ:
		if (InterlockedDecrement16(&pSession->ioRelease.IOCount) == 0)
		{
			ReleaseSession(pSession);
		}
	}
	return;
}

VOID CNetServer::RecvProc(SHORT transferredBytes, stSESSION* pSession)
{
	pSession->recvQ.MoveRear(transferredBytes);

	for (;;)
	{
		stHEADER header;

		int useSize = pSession->recvQ.GetUseSize();

		//��Ʈ��ũ ����� ũ�⺸�� ���� ��� breakŸ�� �ٽ� ���
		if (useSize < sizeof(header))
			break;

		//Peek�� ��� �̰�
		pSession->recvQ.Peek((char*)&header, sizeof(header));

		if (header.length >= QUEUE_SIZE || header.code != 0x77)
		{
			Disconnect(pSession->sessionID);
			break;
		}

		//���� ����� ���ؼ� ��Ŷ�� ���� �����ߴ��� Ȯ���ϱ�
		if (useSize - sizeof(header) < header.length)
			break;

		//����ȭ���� Alloc�ϱ�
		CSerializationBuffer* packet = CSerializationBuffer::Alloc();

		//����ȭ���ۿ� ��Ŷ�� ���� ����
		pSession->recvQ.Dequeue(packet->GetBufferPtr(), header.length + sizeof(header));

		packet->MoveWritePos(header.length);

		//������ �κ�
		if (!CSerializationBuffer::Decode(packet))
		{
			Disconnect(pSession->sessionID);
			packet->DeqRef();
			break;
		}

		//OnRecvȣ���� ���� �������κп� ��Ŷ �����ϱ�
		OnRecv(pSession->sessionID, packet);

		//�۾� �Ϸ� �� ����ȭ������ refCount�� �����ϱ�
		packet->DeqRef();
	}
	InterlockedIncrement64((LONG64*)&recvTPS);
	RecvPost(pSession);
}

VOID CNetServer::SendProc(stSESSION* pSession)
{
	for (int i = 0; i < pSession->sendCount; i++)
		pSession->sendPacketPtrBuf[i]->DeqRef();

	InterlockedExchange((LONG*)&pSession->sendCount, 0);
	InterlockedExchange8((char*)&pSession->sendFlag, false);

	if (pSession->sendQ.GetQueueSize() > 0)
		SendPost(pSession);
}

//VOID CNetServer::WorkerThread()
//{
//	for (;;)
//	{
//		stSESSION* pSession = nullptr;
//		OVERLAPPED* pOverlapped = nullptr;
//		DWORD transferredBytes = 0;
//
//		OVERLAPPED_ENTRY entry[30];
//		ULONG removed;
//		//GetQueuedCompletionStatus(mIocpHandle, &transferredBytes, (PULONG_PTR)&pSession, &pOverlapped, INFINITE);
//		GetQueuedCompletionStatusEx(mIocpHandle, entry, _countof(entry), &removed, INFINITE, FALSE);
//
//		for (int i = 0; i < removed; i++)
//		{
//			if (entry[i].lpOverlapped == nullptr)
//			{
//				PostQueuedCompletionStatus(mIocpHandle, NULL, NULL, NULL);
//				break;
//			}
//
//			pSession = (stSESSION*)entry[i].lpCompletionKey;
//			transferredBytes = entry[i].dwNumberOfBytesTransferred;
//
//			if (entry[i].lpOverlapped == &pSession->recvOverlapped)
//			{
//				pSession->recvQ.MoveRear(transferredBytes);
//
//				for (;;)
//				{
//					stHEADER header;
//
//					int useSize = pSession->recvQ.GetUseSize();
//
//					//��Ʈ��ũ ����� ũ�⺸�� ���� ��� breakŸ�� �ٽ� ���
//					if (useSize < sizeof(header))
//						break;
//
//					//Peek�� ��� �̰�
//					pSession->recvQ.Peek((char*)&header, sizeof(header));
//
//					//���� ����� ���ؼ� ��Ŷ�� ���� �����ߴ��� Ȯ���ϱ�
//					if (useSize - sizeof(header) < header.length)
//						break;
//
//					//��Ƽ��ũ����� �����ϱ�
//					pSession->recvQ.MoveFront(sizeof(header));
//
//					//����ȭ���� Alloc�ϱ�
//					CSerializationBuffer* packet = CSerializationBuffer::Alloc();
//
//					//����ȭ���ۿ� ��Ŷ�� ���� ����
//					pSession->recvQ.Dequeue(packet->GetContentBufPtr(), header.length);
//
//					//OnRecvȣ���� ���� �������κп� ��Ŷ �����ϱ�
//					OnRecv(pSession->sessionID, packet);
//
//					//�۾� �Ϸ� �� ����ȭ������ refCount�� �����ϱ�
//					packet->DeqRef();
//				}
//				RecvPost(pSession);
//			}
//			else if (entry[i].lpOverlapped == &pSession->sendOverlapped)
//			{
//				int sendCount = pSession->sendCount;
//				//�ش� ���ǿ� ���� send�÷��װ� true �� ����
//				CSerializationBuffer* pBuf[MAXWSABUF];
//
//				pSession->sendQ.Dequeue((char*)pBuf, sizeof(nullptr) * sendCount);
//
//				for (int i = 0; i < sendCount; i++)
//					pBuf[i]->DeqRef();
//
//				//pSession->sendFlag = false;
//				InterlockedExchange8((char*)&pSession->sendFlag, false);
//
//				if (pSession->sendQ.GetUseSize() > 0)
//					SendPost(pSession);
//			}
//			else
//			{
//				OnError();
//			}
//
//		IOCOUNT_DEQ:
//			if (InterlockedDecrement16(&pSession->ioRelease.IOCount) == 0)
//			{
//				OnClientLeave(pSession->sessionID);
//				ReleaseSession(pSession);
//			}
//		}
//	}
//	return;
//}

VOID CNetServer::AcceptThread()
{
	for (;;)
	{
		SOCKADDR_IN clientAddr;
		
		int size = sizeof(clientAddr);

		SOCKET clientSocket = accept(mListenSocket, (SOCKADDR*)&clientAddr, &size);
		
		acceptCount++;

		acceptTPS++;

		if (clientSocket == INVALID_SOCKET)
		{
			int errCode = WSAGetLastError();

			//���������� ����Ǿ���
			if (errCode == WSAECONNRESET)
				continue;
			else
			{
				printf("accept error. %d\n", errCode);
				break;
			}
		}

		WCHAR clientIP[46];
		
		InetNtop(AF_INET, (const void*)&clientAddr.sin_addr.s_addr, clientIP, sizeof(clientIP));

		bool acceptFlag = OnConnectionRequest(clientIP, clientAddr.sin_port);
		if (!acceptFlag)
		{
			continue;
		}

		if (mCurrentClientCount == mMaxClientNum)
		{
			closesocket(clientSocket);
			continue;
		}

		InterlockedIncrement(&mCurrentClientCount);

		INT sessionArrayIndex;
		mSessionArrayIndexStack.Pop(&sessionArrayIndex);

		stSESSION* pSession = &mSessionArray[sessionArrayIndex];

		//IOī��Ʈ�� �÷��ִ� ���� : OnClientJoin���� Send�� �ϴµ� Send�� �԰� ���ÿ� �Ϸ������� ��������
		//IOī��Ʈ�� Decrement�ϴµ����� ���ٸ�, ������ Release�� �ɼ��� �ִ�.
		//AcquireSession������ ����ȭ ������ ���� �̰����� ���־�� �Ѵ�.
		InterlockedIncrement16(&pSession->ioRelease.IOCount);

		pSession->socket = clientSocket;
		pSession->socketForRelease = clientSocket;
		pSession->sessionID = MakeSessionID(sessionArrayIndex, mClientID++);
		pSession->sendFlag = false;
		pSession->ioRelease.releaseFlag = 0;
		pSession->sessionArrayIndex = sessionArrayIndex;
		pSession->sendCount = 0;
		pSession->recvQ.ClearBuffer();

		CreateIoCompletionPort((HANDLE)clientSocket, mIocpHandle, (ULONG_PTR)&mSessionArray[sessionArrayIndex], NULL);

		OnClientJoin(mSessionArray[sessionArrayIndex].sessionID);

		RecvPost(&mSessionArray[sessionArrayIndex]);

		if (InterlockedDecrement16(&mSessionArray[sessionArrayIndex].ioRelease.IOCount) == 0)
		{
			ReleaseSession(&mSessionArray[sessionArrayIndex]);
		}
	}
	return;
}

VOID CNetServer::RecvPost(stSESSION* pSession)
{
	DWORD recvQFreeSize = pSession->recvQ.GetFreeSize();
	DWORD recvQDirectEnqueueSize = pSession->recvQ.GetDirectEnqueueSize();

	int wsaBufCount = 1;

	WSABUF dataBuf[2];

	dataBuf[0].buf = pSession->recvQ.GetRearBufferPtr();
	dataBuf[0].len = recvQDirectEnqueueSize;

	if (recvQFreeSize > recvQDirectEnqueueSize)
	{
		wsaBufCount++;
		dataBuf[1].buf = pSession->recvQ.GetStartBufferPtr();
		dataBuf[1].len = recvQFreeSize - recvQDirectEnqueueSize;
	}

	DWORD flags = 0;

	ZeroMemory(&pSession->recvOverlapped, sizeof(pSession->recvOverlapped));
	
	InterlockedIncrement16(&pSession->ioRelease.IOCount);
	int retval = WSARecv(pSession->socket, dataBuf, wsaBufCount, NULL, &flags, &pSession->recvOverlapped, NULL);

	if (retval == SOCKET_ERROR)
	{
		if (WSAGetLastError() == WSA_IO_PENDING)
		{
			//WSA_IO_PENDING�̶��
			//�̹� Recv�� �ɷ��ִ�. Disconnect�� WSARecv�� �ɷ��ִ� ����
			//�ɷȴٸ� Recv�� ������������Ѵ�.
			if (pSession->socket == INVALID_SOCKET)
			{
				//I/Oī��Ʈ ���Ҵ� ABORTED���� ������ �ش�.
				CancelIoEx((HANDLE)pSession->socketForRelease, NULL);
			}
		}
		else
		{
			// ���� ���� ����
			if (InterlockedDecrement16(&pSession->ioRelease.IOCount) == 0)
			{
				ReleaseSession(pSession);
			}
		}
	}
}

bool CNetServer::SendPacket(UINT64 sessionID, CSerializationBuffer* packet)
{
	int enqueueSize;
	stSESSION* pSession;

	pSession = FindSession(sessionID);
	if (pSession == nullptr)
		return false;

	if (!AcquireSession(sessionID, pSession))
		return false;

	packet->AddRef();

	CSerializationBuffer::Encode(packet);

	InterlockedIncrement64((LONG64*)&sendTPS);

	pSession->sendQ.Enqueue(packet);

	SendPost(pSession);

	//useSize�� 0�� ������ ��Ȳ�� ����� SendPost�� �� �� �� ȣ���ϰ��ִ�.
	if (pSession->sendQ.GetQueueSize() > 0)
	{
		SendPost(pSession);
	}

	/*//�� ��ġ�� �ִ� ������, ���� ������ ������ �ְ� IO�Ϸ� �������� ������ IOī��Ʈ ���Һκ��� Ÿ���� ������ SendPacket�� ȣ���Ͽ��� IOī��Ʈ�� �������ѹ��ȴ�.
	//�̷� ���, IO�Ϸ� ���������� IO�� 0�� ���� �������� ���ϰ� ReleaseSession�� Ÿ�� ���Ѵ�. �̷� ��츦 ����Ͽ� SendPacket������ ReleaseSession�� ȣ�������߸��Ѵ�.
	if (InterlockedDecrement16(&pSession->ioRelease.IOCount) == 0)
	{
		ReleaseSession(pSession);
	}*/
	LeaveSession(pSession);

	return true;
}

//SendPost�� �м�
//SendPost�Լ��� ��罺���� ��Ʋ� �� �� �����忡���� ����� �� �ִ�.
//SendPost�� useSize�� 0�� �Ǵ� ��Ȳ�� �� �� �ִ�.
// --> recv�Ϸ� ����Ʈ���� ���� �����͸� ��ť�� �Ѵ�.
// --> ��ť���Ŀ� �����尡 readyť�� ����.
// --> �� ��, �ٸ� send�Ϸ� �������� �ش� transferred��ŭ Dequeue�ϰ�\
//		�� ������ �ִ� if(GetUseSize()) sendPost();�� ȣ���Ѵ�.
// --> �� ��, SendPost�� �Ϸ������� �� �´�. �׷� �ش� transferred�� Dequeue�ȴ�.
// --> �׷� �ٸ� �����尡 Enqueue���� �ʾҴٸ�, ����� 0�� �ɰ��̴�.
// --> �ٽ� readyť���ִ� �����尡 �����.
// --> ���� �����ϰ� SendPost�� ȣ���Ѵ�. �׷� SendPost�ȿ��ִ� useSize�� 0�� ���´�.
//SendPost���� useSize�� 0�� ������ ����� ��ó
// --> ��Ȳ�� �߻��ϴ� ��� :\
		useSize�� 0�� �ȴ�.\
		useSize�� ���ϴ� �ڵ�� ���Ͷ��� �ڵ� ���̿��� �������� ��ť�� �Ѵ�.\
		�ٸ� ������� ��ť�� �ϰ� SendPost�� ȣ���Ѵ�.\
		�ٵ� ���� sendFlag�� true�̱⶧���� SendPost�� �Լ������� ���������Ѵ�.\
		�� �ƹ��͵� �������ϰ� ������ ���͹�����.\
		�� �� �ٽ� ���ƿ� ���Ͷ��� �����ϰ� ��ȯ�Ѵ�.\
		�̷��� �Ǹ� �ᱹ �ٸ� �������� ��ť�� �����ʹ� ���۵��� ���ϰ� ���Եȴ�.\
// --> ù��°�� goto�� ���� ����� �ִ�.
// --> ���� �� ����� SendPost���� GetUseSize�� �� �� �� üũ�� �� �����Ͱ� �����ִٸ� SendPost �� �ٽ� ȣ���Ѵ�.
VOID CNetServer::SendPost(stSESSION* pSession)
{
	char retval = InterlockedExchange8((char*)&pSession->sendFlag, TRUE);

	if (retval == TRUE)
	{
		return;
	}

	auto pSendQ = &pSession->sendQ;

	int useSize = pSendQ->GetQueueSize();

	//{																				//��Ȳ�� �߻��ϴ� ��濡�� ���ϴ� useSize���ϴ� �ڵ�� ���Ͷ� �ڵ��� ����
	if (useSize == 0)
	{
	//}																				//������
		InterlockedExchange8((char*)&pSession->sendFlag, FALSE);
		return;
	}

	WSABUF dataBuf[MAXWSABUF];

	for (int i = 0; i < useSize; i++)
	{
		pSendQ->Dequeue(&pSession->sendPacketPtrBuf[i]);
		dataBuf[i].buf = pSession->sendPacketPtrBuf[i]->GetBufferPtr();
		dataBuf[i].len = pSession->sendPacketPtrBuf[i]->GetTotalUseSize();
	}

	pSession->sendCount = useSize;

	DWORD flags = 0;

	InterlockedIncrement16(&pSession->ioRelease.IOCount);

	ZeroMemory(&pSession->sendOverlapped, sizeof(OVERLAPPED));
	int sendRetval = WSASend(pSession->socket, dataBuf, useSize, NULL, flags, &pSession->sendOverlapped, NULL);

	if (sendRetval == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			// ���� ���� ����
			if (InterlockedDecrement16(&pSession->ioRelease.IOCount) == 0)
			{
				ReleaseSession(pSession);
			}
		}
		else
		{
			if (pSession->socket == INVALID_SOCKET)
			{
				//I/Oī��Ʈ�� ���Ҵ� ABORTED���� �̷�� �����̴�.
				CancelIoEx((HANDLE)pSession->socketForRelease, NULL);
			}
		}
	}
}

CNetServer::stSESSION* CNetServer::FindSession(UINT64 sessionID)
{
	unsigned short index = (unsigned short)sessionID;
	
	//�ѹ� �� ���� ��
	if (mSessionArray[index].sessionID == sessionID)
		return &mSessionArray[index];
	else
		return nullptr;
}

UINT64 CNetServer::MakeSessionID(USHORT sessionArrayIndex, UINT64 sessionUniqueID)
{
	UINT64 sessionID = 0;

	sessionID = sessionUniqueID;

	sessionID = sessionID << 16;

	sessionID += sessionArrayIndex;

	return sessionID;
}

bool CNetServer::ReleaseSession(stSESSION* pSession)
{
	stIO_RELEASE compStruct;
	compStruct.IOCount = 0;
	compStruct.releaseFlag = 0;

	stIO_RELEASE exchangeStruct;
	exchangeStruct.IOCount = 0;
	exchangeStruct.releaseFlag = 1;

	int* pCompareVal = (int*)&exchangeStruct;

	//Release�� �Ȱɷ�(exchangeStruct) ���ϰ� ���� release�� ��ٸ� �ʱⰪ�� 0�� �ƴҰ��̱⶧����
	//�ݺ����� Ż���̰�, release�� �ȉ�ٸ� releaseFlag = 0�̰� iocount�� 0�̱⶧���� 0�� ���´�.
	//��, 0 != Interlocked��� Release�Ȱ�, 0 == Interlocked��� Release�ȵȰ�
	if (0 != InterlockedCompareExchange((LONG*)&pSession->ioRelease, (LONG)*pCompareVal, (LONG)0))
	{
		//�ٸ� ��򰡿��� Release�� �Ǿ���. �� �� �ʿ䰡 ����.
		return false;
	}

	closesocket(pSession->socketForRelease);

	OnClientLeave(pSession->sessionID);

	/*int useSize = pSession->sendQ.GetUseSize();

	int packetCount = useSize / sizeof(nullptr);

	CSerializationBuffer* packetArray[MAXWSABUF];

	pSession->sendQ.Dequeue((char*)packetArray, useSize);

	for (int i = 0; i < packetCount; i++)
	{
		packetArray[i]->DeqRef();
	}*/

	for (int i = 0; i < pSession->sendCount; i++)
	{
		pSession->sendPacketPtrBuf[i]->DeqRef();
	}

	for (;;)
	{
		CSerializationBuffer* pPacket;
		if (!pSession->sendQ.Dequeue(&pPacket))
		{
			break;
		}
		pPacket->DeqRef();
	}

	InterlockedDecrement(&mCurrentClientCount);

	mSessionArrayIndexStack.Push(pSession->sessionArrayIndex);
	
	return true;
}

bool CNetServer::AcquireSession(UINT64 sessionID, stSESSION* pSession)
{
	//IOCount�� ������Ű�� ���� ������ Release�� ������ ����
	//InterlockedIncrement�� ���� �������� �� 0 -> 1 �� �����ߴٰ� ������ ���, Release�� �������� �Ǵ��ϸ�
	//�ȵǴ� ���� = ���� �����忡�� SendPacket�� ���ÿ� ȣ������ ��쿡 1�� �ƴ� 2, 3, 4, 5 �� �̻��� �ɼ��ִ�.
	//��Ƽ�����忡���� �ǹ̰� ����.
	//InterlockedIncrement16(&pSession->ioRelease.IOCount);

	//�㳪 ���࿡ ���� �˴� sessionID�� �����ͷ� ������ ���Ǿ��̵�� �ٸ��ٸ� ������ �ٸ�����
	//if (pSession->sessionID != sessionID)
	//{
		//IOCount�� 0�̰� Release�� �� ������ �ƴ϶��
		//if (InterlockedDecrement16(&pSession->ioRelease.IOCount) == 0 && pSession->ioRelease.releaseFlag == 0)
		//{
			//���� �÷��������ν� ������ ������ �ȉ��������ֱ⶧���� ���������ÿ� 0üũ�� Release����
			//ReleaseSession(pSession);
		//}
		//return false;
	//}

	InterlockedIncrement16(&pSession->ioRelease.IOCount);
	

	if (pSession->ioRelease.releaseFlag == true || pSession->sessionID != sessionID)
	{
		LeaveSession(pSession);
		
		return false;
	}

	return true;
}

void CNetServer::LeaveSession(stSESSION* pSession)
{
	if (InterlockedDecrement16(&pSession->ioRelease.IOCount) == 0)
	{
		ReleaseSession(pSession);
	}
}

CNetServer::CNetServer()
	: mListenSocket(INVALID_SOCKET)
	, mServerPort(NULL)
	, mMaxClientNum(NULL)
	, mCurrentClientCount(NULL)
	, mWorkerThreadRunNum(NULL)
	, mWorkerThreadCreateNum(NULL)
	, mClientID(NULL)
	, mIocpHandle(INVALID_HANDLE_VALUE)
	, mThreadArr(nullptr)
	, mSessionArray(nullptr) {}

bool CNetServer::Start(const wchar_t* ip, short port, int workerThreadRunNum, int workerThreadCreateNum, bool nagleOpt, int maxClientNum)
{
	int retval;

	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		return false;
	}

	mListenSocket = socket(AF_INET, SOCK_STREAM, NULL);
	
	if (mListenSocket == INVALID_SOCKET)
	{
		return false;
	}

	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;
	InetPton(AF_INET, ip, &serverAddr.sin_addr);
	serverAddr.sin_port = htons(port);

	retval = bind(mListenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
	if (retval == SOCKET_ERROR)
	{
		return false;
	}

	//���̱� �ɼ�
	bool optval = nagleOpt;
	retval = setsockopt(mListenSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&optval, sizeof(optval));
	if (retval == SOCKET_ERROR)
	{
		return false;
	}

	linger lingerOpt;
	lingerOpt.l_onoff = 1;  // ���ſɼ� 1 : on, 0 : off
	lingerOpt.l_linger = 0; // RST ������ ���� 0���� ����

	retval = setsockopt(mListenSocket, SOL_SOCKET, SO_LINGER, (const char*)&lingerOpt, sizeof(lingerOpt));
	if (retval == SOCKET_ERROR)
	{
		printf("%d\n", WSAGetLastError());
		return false;
	}

	mIocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, workerThreadRunNum);

	// +1�� AcceptThread
	//mThreadArr = (HANDLE*)malloc(sizeof(HANDLE) * workerThreadCreateNum + 1);
	
	mThreadArr = new HANDLE[workerThreadCreateNum + 1];

	mMaxClientNum = maxClientNum;
	

	mSessionArray = new stSESSION[maxClientNum];

	//�ε������� �� ��´�.
	for (int sessionCount = 0; sessionCount < maxClientNum; sessionCount++)
	{
		mSessionArrayIndexStack.Push(sessionCount);
	}

	retval = listen(mListenSocket, SOMAXCONN);
	if (retval == SOCKET_ERROR)
	{
		return false;
	}

	int threadCount = 0;

	for (threadCount = 0; threadCount < workerThreadCreateNum; threadCount++)
	{
		mThreadArr[threadCount] = (HANDLE)_beginthreadex(NULL, NULL, StaticWorkerThread, (PVOID)this, NULL, NULL);
	}

	mThreadArr[threadCount] = (HANDLE)_beginthreadex(NULL, NULL, StaticAcceptThread, (PVOID)this, NULL, NULL);

	return true;
}

VOID CNetServer::Stop()
{
	//���ο� �����ڸ� �������� �������� �ݱ�, ���⼭ AcceptThread�� �����ϰ� �ȴ�.
	closesocket(mListenSocket);

	for (int clientNum = 0; clientNum < mMaxClientNum; clientNum++)
	{
		//if (mSessionArray[clientNum].occupyFlag == false)
			//continue;
		//ReleaseSession(&mSessionArray[clientNum]);
		Disconnect(mSessionArray[clientNum].sessionID);
	}

	for (;;)
	{
		//������ ���� �ݳ���
		//if (mSessionArrayIndexStack.() == mMaxClientNum)
			//break;

		Sleep(100);
	}

	//WorkerThread�� ��������� ���� ������ �ɰ���
	PostQueuedCompletionStatus(mIocpHandle, NULL, NULL, NULL);

	std::list<HANDLE> notTerminatedThreadList;

	//mWorkerThreadCreateNum + 1 = ��Ŀ�����尹�� + ���Ʈ������
	for (int threadIndex = 0; threadIndex < mWorkerThreadCreateNum + 1; threadIndex++)
	{
		DWORD retval = WaitForSingleObject(mThreadArr[threadIndex], 1000);
		if (retval == WAIT_TIMEOUT)
		{
			notTerminatedThreadList.push_back(mThreadArr[threadIndex]);
		}
	}

	for (auto listItor = notTerminatedThreadList.begin(); listItor != notTerminatedThreadList.end(); ++listItor)
	{
		DWORD retval = WaitForSingleObject(*listItor, 0);
		if (retval == WAIT_TIMEOUT)
		{
			TerminateThread(*listItor, 1);
		}
	}

	//������迭 ����
	delete mThreadArr;
	//���ǹ迭 ����
	delete[] mSessionArray;
}

INT CNetServer::GetClientCount()
{
	return mCurrentClientCount;
}

UINT64 CNetServer::GetAcceptCount()
{
	return acceptCount;
}

bool CNetServer::Disconnect(UINT64 sessionID)
{
	stSESSION* pSession;

	pSession = FindSession(sessionID);
	if (pSession == nullptr)
	{
		return false;
	}

	if (!AcquireSession(sessionID, pSession))
	{
		return false;
	}
	
	if (InterlockedExchange64((LONG64*)&pSession->socket, INVALID_SOCKET) == INVALID_SOCKET)
	{
		LeaveSession(pSession);
		return false;
	}

	CancelIoEx((HANDLE)pSession->socketForRelease, NULL);

	LeaveSession(pSession);

	return true;
}