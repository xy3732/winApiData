#define MAIN
#include "Common.h"

int main(int argc, char *argv[])
{
	int retval;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

	// ����� �Ϸ� ��Ʈ ����
	HANDLE hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hcp == NULL) return 1;

	InitializeCriticalSection(&cs);
	// CPU ���� Ȯ��
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	// (CPU ���� * 2)���� �۾��� ������ ����
	HANDLE hThread;
	for (int i = 0; i < (int)si.dwNumberOfProcessors * 2; i++) {
		hThread = CreateThread(NULL, 0, WorkerThread, hcp, 0, NULL);
		if (hThread == NULL) return 1;
		CloseHandle(hThread);
	}

	// ���� ����
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");

	// ������ ��ſ� ����� ����
	SOCKET client_sock;
	struct sockaddr_in clientaddr;
	int addrlen;
	DWORD recvbytes, flags;

	while (1) {
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (struct sockaddr *)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET) {
			err_display("accept()");
			break;
		}

		_ClientInfo* ptr = AddClientInfo(client_sock, clientaddr);
		
		if (!ptr)
		{			
			continue;
		}

		// ���ϰ� ����� �Ϸ� ��Ʈ ����
		CreateIoCompletionPort((HANDLE)client_sock, hcp, client_sock, 0);

		if (!Recv(ptr))
		{
			RemoveClientInfo(ptr);
		}

		InitProcess(ptr);
	}

	DeleteCriticalSection(&cs);
	// ���� ����
	WSACleanup();
	return 0;
}

// �۾��� ������ �Լ�
DWORD WINAPI WorkerThread(LPVOID arg)
{
	int retval;
	HANDLE hcp = (HANDLE)arg;

	while (1)
	{
		// �񵿱� ����� �Ϸ� ��ٸ���
		DWORD cbTransferred;
		SOCKET client_sock;
		EXOVERLAPPED* overlapped;

		retval = GetQueuedCompletionStatus(hcp, &cbTransferred,
			(PULONG_PTR)&client_sock, (LPOVERLAPPED *)&overlapped, INFINITE);

		_ClientInfo* ptr = overlapped->ptr;
		IO_TYPE type = overlapped->type;
				// �񵿱� ����� ��� Ȯ��
		if (retval == 0 || cbTransferred == 0)
		{
			DWORD temp1, temp2;
			int result = WSAGetOverlappedResult(ptr->sock, &overlapped->overlapped, &temp1, false, &temp1);
			if (result==0)
			{
				err_display(WSAGetLastError());
			}
			//Ŭ���̾�Ʈ ���� ����
			DisconnectedProcess(ptr);			
			continue;
		}

		switch (type)
		{
		case IO_TYPE::RECV:
			retval = CompleteRecv(ptr, cbTransferred);
			switch (retval)
			{
			case SOC_ERROR:
			case SOC_FALSE:
				continue;
			case SOC_TRUE:
				break;
			}
			RecvPacketProcess(ptr);
			if (!Recv(ptr))
			{
				RemoveClientInfo(ptr);				
			}
			break;
		case IO_TYPE::SEND:
			retval = CompleteSend(ptr, cbTransferred);
			switch (retval)
			{
			case SOC_ERROR:
			case SOC_FALSE:
				continue;
			case SOC_TRUE:
				break;
			}
			SendPacketProcess(ptr);
			break;
		}	
	}
	return 0;
}