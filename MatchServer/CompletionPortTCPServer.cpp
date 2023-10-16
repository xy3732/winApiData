#define MAIN
#include "Common.h"

int main(int argc, char *argv[])
{
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

	// 입출력 완료 포트 생성
	HANDLE hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hcp == NULL) return 1;

	InitializeCriticalSection(&cs);
	// CPU 개수 확인
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	// (CPU 개수 * 2)개의 작업자 스레드 생성
	HANDLE hThread;
	for (int i = 0; i < (int)si.dwNumberOfProcessors * 2; i++) {
		hThread = CreateThread(NULL, 0, WorkerThread, hcp, 0, NULL);
		if (hThread == NULL) return 1;
		CloseHandle(hThread);
	}

	// 소켓 생성
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

	// 데이터 통신에 사용할 변수
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

		// 소켓과 입출력 완료 포트 연결
		CreateIoCompletionPort((HANDLE)client_sock, hcp, client_sock, 0);

		if (!Recv(ptr))
		{
			RemoveClientInfo(ptr);
		}

		InitProcess(ptr);
	}

	DeleteCriticalSection(&cs);
	// 윈속 종료
	WSACleanup();
	return 0;
}

// 작업자 스레드 함수
DWORD WINAPI WorkerThread(LPVOID arg)
{
	int retval;
	HANDLE hcp = (HANDLE)arg;

	while (1)
	{
		// 비동기 입출력 완료 기다리기
		DWORD cbTransferred;
		SOCKET client_sock;
		EXOVERLAPPED* overlapped;

		retval = GetQueuedCompletionStatus(hcp, &cbTransferred,
			(PULONG_PTR)&client_sock, (LPOVERLAPPED *)&overlapped, INFINITE);

		_ClientInfo* ptr = overlapped->ptr;
		IO_TYPE type = overlapped->type;
				// 비동기 입출력 결과 확인
		if (retval == 0 || cbTransferred == 0)
		{
			DWORD temp1, temp2;
			int result = WSAGetOverlappedResult(ptr->sock, &overlapped->overlapped, &temp1, false, &temp1);
			if (result==0)
			{
				err_display(WSAGetLastError());
			}
			//클라이언트 연결 종료
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