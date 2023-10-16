#define _CRT_SECURE_NO_WARNINGS // 구형 C 함수 사용 시 경고 끄기
#define _WINSOCK_DEPRECATED_NO_WARNINGS // 구형 소켓 API 사용 시 경고 끄기

#include <windows.h>
#include "resource.h"

#pragma comment(lib, "ws2_32") // ws2_32.lib 링크

#define random(n) (rand()%n)
#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000
#define BUFSIZE    512

enum PROTOCOL
{
	WAIT = 1,
	INTRO,
	PLAYER_INFO,
	CLICK_POSITION,
	HIDE_TEMP_CARD,
	MY_CLICK,
	OPPONENET_CLICK,
	RESULT,
	INVALID_GAME
};

enum CARD_STATE
{
	HIDDEN = 1,
	FLIP,
	TEMPFLIP
};

struct CARD_INFO
{
	int Num;
	CARD_STATE State;
};

enum { INIT, RUN, HINT, VIEW } GameStatus;

LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
HINSTANCE g_hInst;
HWND hWndMain;
LPCTSTR lpszClass=TEXT("Couple2");
SOCKET sock; // 소켓
char send_buf[BUFSIZE + 1]; // 데이터 송신 버퍼
char recv_buf[BUFSIZE + 1]; // 데이터 수신 버퍼

CARD_INFO PlayerCards[4][4];
CARD_INFO OpponentCards[4][4];
HBITMAP hShape[9];

char Msgbuf[512];
int Myremain = 16;
int Eremain = 16;

void DrawScreen(HDC hdc);
int GetTempFlipCount();
void DrawBitmap(HDC hdc, int x, int y, HBITMAP hBit);

DWORD WINAPI ClientMain(LPVOID arg);

void DisplayError(const char* msg);

PROTOCOL GetProtocol(char* _buf);
int PackPacket(char* _buf, PROTOCOL _protocol, int _data1, int _data2);
int PackPacket(char* _buf, PROTOCOL _protocol);
void UnPackPacket(char* _buf, CARD_INFO _data[4][4], int &_remain);
void UnPackPacket(char* _buf, char* _str1);
void UnPackPacket(char* _buf, char* _str1, CARD_INFO _data[4][4]);

int recvn(SOCKET sock, char* buf, int len, int flags);
bool PacketRecv(SOCKET _sock, char* _buf);


int APIENTRY WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance
	  ,LPSTR lpszCmdParam,int nCmdShow)
{
	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	HWND hWnd;
	MSG Message;
	WNDCLASS WndClass;
	g_hInst=hInstance;
	
	WndClass.cbClsExtra=0;
	WndClass.cbWndExtra=0;
	WndClass.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
	WndClass.hCursor=LoadCursor(NULL,IDC_ARROW);
	WndClass.hIcon=LoadIcon(g_hInst,MAKEINTRESOURCE(IDI_COUPLE));
	WndClass.hInstance=hInstance;
	WndClass.lpfnWndProc=WndProc;
	WndClass.lpszClassName=lpszClass;
	WndClass.lpszMenuName=NULL;
	WndClass.style=0;
	RegisterClass(&WndClass);

	hWnd=CreateWindow(lpszClass,lpszClass, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT,CW_USEDEFAULT,0,0,NULL,(HMENU)NULL,hInstance,NULL);
	ShowWindow(hWnd,nCmdShow);
	
	// 소켓 통신 스레드 생성
	CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);

	while (GetMessage(&Message,NULL,0,0)) {
		TranslateMessage(&Message);
		DispatchMessage(&Message);
	}
	return (int)Message.wParam;
}

// TCP 클라이언트 시작
DWORD WINAPI ClientMain(LPVOID arg)
{
	int retval;

	// 소켓 생성
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET)
	{
		DisplayError("socket()");
		return 0;
	}

	// connect()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
	{
		DisplayError("connect()");
		return 0;
	}

	int size;
	char msg[BUFSIZE];
	PROTOCOL protocol;
	bool endflag = false;

	while (true)
	{
		if (!PacketRecv(sock, recv_buf))
		{
			break;
		}

		PROTOCOL protocol = GetProtocol(recv_buf);

		switch (protocol)
		{
		case WAIT:
		case PLAYER_INFO:
			//받은 메세지 출력
			memset(Msgbuf, 0, sizeof(Msgbuf));
			UnPackPacket(recv_buf, Msgbuf);
			InvalidateRect(hWndMain, NULL, FALSE);
			break;
		case INTRO:
			//받은 메세지 출력
			memset(Msgbuf, 0, sizeof(Msgbuf));
			UnPackPacket(recv_buf, Msgbuf, PlayerCards);

			//게임 시작시 모든 카드를 2초간 보여준뒤 카드를 뒤집을 수 있게 해줌
			GameStatus = HINT;
			InvalidateRect(hWndMain, NULL, TRUE);
			SetTimer(hWndMain, 0, 2000, NULL);
			break;
		case MY_CLICK:
			//클릭 위치에 대한 결과 수령
			UnPackPacket(recv_buf, PlayerCards, Myremain);

			//임시카드가 없음 = 세트 맞춤
			if (GetTempFlipCount() == 0)
			{
				MessageBeep(0);
			}
			//임시카드가 두개 = 세트 맞추기 실패
			else if (GetTempFlipCount() == 2)
			{
				//다른 카드 못 뒤집게 막고 1초뒤에 임시카드 가리면서 풀어줌
				GameStatus = VIEW;
				SetTimer(hWndMain, 1, 1000, NULL);
			}
			InvalidateRect(hWndMain, NULL, FALSE);
			break;
		case OPPONENET_CLICK:
			//상대방이 클릭한 위치에 대한 결과 수령
			UnPackPacket(recv_buf, OpponentCards, Eremain);
			InvalidateRect(hWndMain, NULL, FALSE);
			break;
		case INVALID_GAME:
			//받은 메세지 출력
			memset(Msgbuf, 0, sizeof(Msgbuf));
			UnPackPacket(recv_buf, Msgbuf);
			InvalidateRect(hWndMain, NULL, FALSE);
			//5초뒤 자동으로 창 종료
			SetTimer(hWndMain, 2, 5000, NULL);
			endflag = true;
			break;
		case RESULT:
			//결과 메세지 출력
			memset(Msgbuf, 0, sizeof(Msgbuf));
			UnPackPacket(recv_buf, Msgbuf);
			InvalidateRect(hWndMain, NULL, FALSE);
			//5초뒤 자동으로 창 종료
			SetTimer(hWndMain, 2, 5000, NULL);
			endflag = true;
			break;
		}

		if (endflag)
		{
			break;
		}
	}
	return 0;
}

//리시브쓰레드에서 인트로프로토콜 받으면 메세지 출력하고 2초 타이머 돌리고 해당 타이머에서 2초간 보여주는 타이머 또 돌리면서 게임 시작
LRESULT CALLBACK WndProc(HWND hWnd,UINT iMessage,WPARAM wParam,LPARAM lParam)
{
	HDC hdc;
	PAINTSTRUCT ps;
	int nx,ny;
	int i,j;
	int tx,ty;
	RECT crt;
	int size, retval;

	switch (iMessage) 
	{
	case WM_CREATE:
		SetRect(&crt, 0, 0, 64 * 4 + 230 + 64 * 4, 64 * 4 + 50);
		AdjustWindowRect(&crt,WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,FALSE);
		SetWindowPos(hWnd,NULL,0,0,crt.right-crt.left,crt.bottom-crt.top,
			SWP_NOMOVE | SWP_NOZORDER);
		hWndMain=hWnd;
		for (i=0;i<sizeof(hShape)/sizeof(hShape[0]);i++) 
		{
			hShape[i]=LoadBitmap(g_hInst,MAKEINTRESOURCE(IDB_SHAPE1+i));
		}
		ZeroMemory(&Msgbuf, 512);
		//기다리는동안 카드판이 물음표로 보이도록 초기화
		for (i = 0; i < 4; i++)
		{
			for (j = 0; j < 4; j++)
			{
				PlayerCards[i][j].State = HIDDEN;
				OpponentCards[i][j].State = HIDDEN;
			}
		}
		GameStatus = INIT;
		return 0;
	case WM_LBUTTONDOWN:
		nx=LOWORD(lParam)/64;
		ny=HIWORD(lParam)/64;

		//뒤집기 가능한 상태가 아니거나 클릭 위치가 그림이 아니거나 이미 밝혀진 그림을 클릭하면 아무일도 일어나지 않음
		if (GameStatus != RUN || nx > 3 || ny > 3 || PlayerCards[nx][ny].State != HIDDEN) 
		{
			return 0;
		}

		//클릭 위치 전송
		size = PackPacket(send_buf, CLICK_POSITION, nx, ny);
		retval = send(sock, send_buf, size, 0);
		if (retval == SOCKET_ERROR)
		{
			DisplayError("Click_Send()");
			return -1;
		}
		return 0;
	case WM_TIMER:
		switch (wParam) 
		{
		case 0:
			//게임 시작
			KillTimer(hWnd,0);
			GameStatus=RUN;
			InvalidateRect(hWnd,NULL,FALSE);
			break;
		case 1:
			KillTimer(hWnd,1);
			//본인 화면에서 임시카드 가리고 카드 뒤집을수 있게 함
			GameStatus=RUN;
			for (i=0;i<4;i++) 
			{
				for (j=0;j<4;j++) 
				{
					if (PlayerCards[i][j].State==TEMPFLIP) 
						PlayerCards[i][j].State=HIDDEN;
				}
			}
			//상대 화면에서도 임시카드 가리라고 신호 전송
			size = PackPacket(send_buf, HIDE_TEMP_CARD);
			retval = send(sock, send_buf, size, 0);
			if (retval == SOCKET_ERROR)
			{
				DisplayError("Click_Send()");
				return -1;
			}
			InvalidateRect(hWnd,NULL,FALSE);
			break;
		case 2:
			KillTimer(hWnd, 2);

			// closesocket()
			closesocket(sock);

			// 윈속 종료
			WSACleanup();

			//윈도우창 파괴
			DestroyWindow(hWndMain);
			break;
		}
		return 0;
	case WM_PAINT:
		hdc=BeginPaint(hWnd, &ps);
		DrawScreen(hdc);
		EndPaint(hWnd, &ps);
		return 0;
	case WM_DESTROY:
		for (i=0;i<sizeof(hShape)/sizeof(hShape[0]);i++) 
		{
			DeleteObject(hShape[i]);
		}
		PostQuitMessage(0);
		return 0;
	}
	return(DefWindowProc(hWnd,iMessage,wParam,lParam));
}

void DrawScreen(HDC hdc)
{
	int x, y, image;
	TCHAR Mes[128];

	//현재 플레이어 카드판 그림
	for (x = 0; x < 4; x++)
	{
		for (y = 0; y < 4; y++)
		{
			if (GameStatus == HINT || PlayerCards[x][y].State != HIDDEN)
			{
				//힌트상태라면 그림을 보여줌
				//숨김상태가 아닌 칸은 배정받은 그림을 보여줌
				image = PlayerCards[x][y].Num - 1;
			}
			else
			{
				//그게 아니라면 물음표그림을 보여줌
				image = 8;
			}
			DrawBitmap(hdc, x * 64, y * 64, hShape[image]);
		}
	}

	//상대 플레이어 카드판 그림
	for (x = 0; x < 4; x++)
	{
		for (y = 0; y < 4; y++)
		{
			if (OpponentCards[x][y].State != HIDDEN)
			{
				//힌트상태라면 그림을 보여줌
				//숨김상태가 아닌 칸은 배정받은 그림을 보여줌
				image = OpponentCards[x][y].Num - 1;
			}
			else
			{
				//그게 아니라면 물음표그림을 보여줌
				image = 8;
			}
			DrawBitmap(hdc, x * 64 + 486, y * 64, hShape[image]);
		}
	}

	lstrcpy(Mes, "짝 맞추기 온라인");
	TextOut(hdc, 300, 10, Mes, lstrlen(Mes));
	//서버에서 받아온 메세지 출력
	wsprintf(Mes, Msgbuf);
	TextOut(hdc, 300, 50, Mes, lstrlen(Mes));
	wsprintf(Mes, "내 남은 카드 수 : %d   ", Myremain);
	TextOut(hdc, 300, 70, Mes, lstrlen(Mes));
	wsprintf(Mes, "상대 남은 카드 수 : %d   ", Eremain);
	TextOut(hdc, 300, 90, Mes, lstrlen(Mes));
	//어떤 카드판이 누구의 것인지 가시화
	lstrcpy(Mes, "Me");
	TextOut(hdc, 120, 270, Mes, lstrlen(Mes));
	lstrcpy(Mes, "Opponent");
	TextOut(hdc, 585, 270, Mes, lstrlen(Mes));
}

//임시카드 갯수 세서 리턴하는 함수
int GetTempFlipCount()
{
	int i, j;
	int count = 0;

	for (i=0;i<4;i++) 
	{
		for (j=0;j<4;j++) 
		{
			if (PlayerCards[i][j].State == TEMPFLIP)
			{
				count++;
			}
		}
	}
	return count;
}

void DrawBitmap(HDC hdc,int x,int y,HBITMAP hBit)
{
	HDC MemDC;
	HBITMAP OldBitmap;
	int bx,by;
	BITMAP bit;

	MemDC=CreateCompatibleDC(hdc);
	OldBitmap=(HBITMAP)SelectObject(MemDC, hBit);

	GetObject(hBit,sizeof(BITMAP),&bit);
	bx=bit.bmWidth;
	by=bit.bmHeight;

	BitBlt(hdc,x,y,bx,by,MemDC,0,0,SRCCOPY);

	SelectObject(MemDC,OldBitmap);
	DeleteDC(MemDC);
}

// 소켓 함수 오류 출력
void DisplayError(const char* msg)
{
	LPVOID lpMsgBuf;
	char Mes[1024];
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(char*)&lpMsgBuf, 0, NULL);
	wsprintf(Mes, "[%s] %s\r\n", msg, (char*)lpMsgBuf);
	MessageBox(hWndMain, Mes, "에러발생", MB_OK);
	LocalFree(lpMsgBuf);
}

int recvn(SOCKET sock, char* buf, int len, int flags)
{
	int received;
	char* ptr = buf;
	int left = len;

	while (left > 0)
	{
		received = recv(sock, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;

		left -= received;
		ptr += received;
	}

	return len - left;
}

bool PacketRecv(SOCKET _sock, char* _buf)
{
	int size;

	int retval = recvn(_sock, (char*)&size, sizeof(size), 0);
	if (retval == SOCKET_ERROR)
	{
		DisplayError("recv error()");
		return false;
	}
	else if (retval == 0)
	{
		return false;
	}

	retval = recvn(_sock, _buf, size, 0);
	if (retval == SOCKET_ERROR)
	{
		DisplayError("recv error()");
		return false;

	}
	else if (retval == 0)
	{
		return false;
	}

	return true;
}

PROTOCOL GetProtocol(char* _buf)
{
	PROTOCOL protocol;
	memcpy(&protocol, _buf, sizeof(PROTOCOL));

	return protocol;
}

int PackPacket(char* _buf, PROTOCOL _protocol, int _data1, int _data2)
{
	char* ptr = _buf;
	int size = 0;

	ptr = ptr + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(_protocol);

	memcpy(ptr, &_data1, sizeof(_data1));
	ptr = ptr + sizeof(_data1);
	size = size + sizeof(_data1);

	memcpy(ptr, &_data2, sizeof(_data2));
	ptr = ptr + sizeof(_data2);
	size = size + sizeof(_data2);

	ptr = _buf;
	memcpy(ptr, &size, sizeof(size));

	size = size + sizeof(size);

	return size;
}

int PackPacket(char* _buf, PROTOCOL _protocol)
{
	char* ptr = _buf;
	int size = 0;

	ptr = ptr + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(_protocol);

	ptr = _buf;
	memcpy(ptr, &size, sizeof(size));

	size = size + sizeof(size);

	return size;
}

void UnPackPacket(char* _buf, CARD_INFO _data[4][4], int &_remain)
{
	char* ptr = _buf + sizeof(PROTOCOL);

	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			memcpy(&_data[i][j], ptr, sizeof(_data[i][j]));
			ptr = ptr + sizeof(_data[i][j]);
		}
	}

	memcpy(&_remain, ptr, sizeof(_remain));
	ptr = ptr + sizeof(_remain);
}

void UnPackPacket(char* _buf, char* _str1)
{
	int strsize1;

	char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&strsize1, ptr, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);

	memcpy(_str1, ptr, strsize1);
	ptr = ptr + strsize1;
}

void UnPackPacket(char* _buf, char* _str1, CARD_INFO _data[4][4])
{
	int strsize1;

	char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&strsize1, ptr, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);

	memcpy(_str1, ptr, strsize1);
	ptr = ptr + strsize1;

	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			memcpy(&_data[i][j], ptr, sizeof(_data[i][j]));
			ptr = ptr + sizeof(_data[i][j]);
		}
	}
}
