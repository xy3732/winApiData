
#pragma comment(lib, "ws2_32") // ws2_32.lib ��ũ

#define _CRT_SECURE_NO_WARNINGS // ���� C �Լ� ��� �� ��� ����
#define _WINSOCK_DEPRECATED_NO_WARNINGS // ���� ���� API ��� �� ��� ����

#include <winsock2.h> // ����2 ���� ���
#include <ws2tcpip.h> // ����2 Ȯ�� ���

#include <tchar.h> // _T(), ...
#include <stdio.h> // printf(), ...
#include <stdlib.h> // exit(), ...
#include <string.h> // strncpy(), ...
#include <time.h>
#include <tchar.h>
#include <queue>
using namespace std;

#define SERVERPORT 9000
#define BUFSIZE    4096
#define PLAYER_COUNT 2

#define WAIT_MSG "��Ī���Դϴ�..."
#define INTRO_MSG "������ �����մϴ�."
#define WIN_MSG "����� �¸��Դϴ�!"
#define LOSE_MSG "����� �����ϴ�.��.��"

enum CLIENT_STATE
{
	C_INIT_STATE = 1,
	C_GAME_INIT_STATE,
	C_PLAYING_STATE,
	C_GAME_OVER_STATE,
	C_DISCONNECTED_STATE
};

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


struct _ClientInfo;

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

enum GAME_STATE
{
	G_WAIT_STATE = 1,
	G_PLAYING_STATE,
	G_HINT_STATE,
	G_VIEW_STATE,
	G_GAME_OVER_STATE,
	G_INVALID_GAME_STATE
};

struct _GameInfo
{
	int game_number;	
	GAME_STATE state;
	_ClientInfo* players[PLAYER_COUNT];
	CARD_INFO Cards[2][4][4];	//�� �÷��̾��� ī���� ����
	int remain[2];
	int player_count;
	bool full;
};

enum IO_TYPE
{
	RECV = 1,
	SEND
};

struct EXOVERLAPPED
{
	WSAOVERLAPPED overlapped;
	_ClientInfo* ptr;
	IO_TYPE	type;
};

enum
{
	SOC_ERROR = 1,
	SOC_TRUE,
	SOC_FALSE
};

struct _SendPacket
{
	char		 sendbuf[BUFSIZE];
	int			 sendbytes;
	int			 comp_sendbytes;
	WSABUF		 s_wsabuf;
	EXOVERLAPPED s_overlapped;
};

struct _ClientInfo
{
	EXOVERLAPPED r_overlapped;
	
	SOCKET		 sock;
	SOCKADDR_IN  addr;	
	
	CLIENT_STATE state;
	int			 player_number;
	_GameInfo*	 game_info;

	bool		 r_sizeflag; 
	int			 recvbytes;
	int			 comp_recvbytes;	
	char		 recvbuf[BUFSIZE];	
	WSABUF		 r_wsabuf;	

	queue<_SendPacket*>* SendQueue;
};

PROTOCOL GetProtocol(const char* _ptr);
int PackPacket(char*, PROTOCOL, const char*);
int PackPacket(char* _buf, PROTOCOL  _protocol, const char* _str, CARD_INFO _data[4][4]);
int PackPacket(char* _buf, PROTOCOL _protocol, CARD_INFO _data[4][4], int _remain);
void UnPackPacket(char*, int&, int&);

_ClientInfo* AddClientInfo(SOCKET _sock, SOCKADDR_IN _addr);
void RemoveClientInfo(_ClientInfo* _ptr);

_GameInfo* _AddGameInfo(_ClientInfo*);
void ReMoveGameInfo(_GameInfo*);

void RecvPacketProcess(_ClientInfo* _ptr);
void SendPacketProcess(_ClientInfo* _ptr);

void err_quit(const char* msg);
void err_display(const char* msg);
void err_display(int errcode);

bool Recv(_ClientInfo* _ptr);
int CompleteRecv(_ClientInfo* _ptr, int cbTransferred);
bool Send(_ClientInfo* _ptr, char* _buf, int size);
int CompleteSend(_ClientInfo* _ptr, int cbTransferred);
DWORD WINAPI WorkerThread(LPVOID arg);

void InitProcess(_ClientInfo*);
void SetCards(CARD_INFO cards[4][4]);
void GetTempFlip(int* tx, int* ty, CARD_INFO cards[4][4]);
void PlayProcess(_ClientInfo*);
void OtherTurnProcess(_ClientInfo*);
void InvalidGameProcess(_ClientInfo*);
void DisconnectedProcess(_ClientInfo*);
_ClientInfo* SearchNextClient(_ClientInfo*);
void UpdateGameInfo(_ClientInfo* _ptr);
void ExitClientCheck(_ClientInfo*);


#ifdef MAIN
_ClientInfo* ClientInfo[100];
int Count = 0;

_GameInfo* GameInfo[100];
int GameCount = 0;

CRITICAL_SECTION cs;

#else

extern _ClientInfo* ClientInfo[100];
extern int Count;

extern _GameInfo* GameInfo[100];
extern int GameCount;

extern CRITICAL_SECTION cs;
#endif

