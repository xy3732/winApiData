#include "Common.h"

//유저정보 추가
_ClientInfo* AddClientInfo(SOCKET _sock, SOCKADDR_IN _clientaddr)
{
	EnterCriticalSection(&cs);
	//소켓 구조체 배열에 새로운 소켓 정보 구조체 저장
	if (Count >= 100)
	{
		printf("더이상 세션을 생성할 수 없습니다.");
		return nullptr;
	}
	_ClientInfo* ptr = new _ClientInfo;
	ZeroMemory(ptr, sizeof(_ClientInfo));

	ptr->state = CLIENT_STATE::C_INIT_STATE;
	ptr->player_number = -1;

	ptr->sock = _sock;
	memcpy(&ptr->addr, &_clientaddr, sizeof(SOCKADDR_IN));
	ptr->r_sizeflag = false;
	ptr->recvbytes = sizeof(int);

	ptr->r_overlapped.ptr = ptr;
	ptr->r_overlapped.type = IO_TYPE::RECV;

	ptr->SendQueue = new queue<_SendPacket*>();
		

	ClientInfo[Count++] = ptr;

	LeaveCriticalSection(&cs);

	printf("\nClient 접속: IP 주소=%s, 포트 번호=%d\n", inet_ntoa(_clientaddr.sin_addr),
		ntohs(_clientaddr.sin_port));

	return ptr;
}

//유저정보 제거
void RemoveClientInfo(_ClientInfo* _ptr)
{

	printf("\nClient 종료: IP 주소=%s, 포트 번호=%d\n", inet_ntoa(_ptr->addr.sin_addr), ntohs(_ptr->addr.sin_port));

	EnterCriticalSection(&cs);

	for (int i = 0; i < Count; i++)
	{
		if (ClientInfo[i] == _ptr)
		{
			closesocket(ClientInfo[i]->sock);
			delete ClientInfo[i]->SendQueue;
			delete ClientInfo[i];

			for (int j = i; j < Count - 1; j++)
			{
				ClientInfo[j] = ClientInfo[j + 1];
			}

			ClientInfo[Count - 1] = nullptr;
			Count--;
			break;
		}
	}

	LeaveCriticalSection(&cs);
}

_GameInfo* _AddGameInfo(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);

	_GameInfo* game_ptr = nullptr;
	int index = -1;

	//게임방에 유저정보 추가
	for (int i = 0; i < GameCount; i++)
	{
		if (!GameInfo[i]->full)
		{
			GameInfo[i]->players[GameInfo[i]->player_count++] = _ptr;
			_ptr->game_info = GameInfo[i];
			_ptr->player_number = GameInfo[i]->player_count;
			//새로 들어온 플레이어의 카드판 배치
			SetCards(GameInfo[i]->Cards[_ptr->player_number -1]);
			GameInfo[i]->remain[_ptr->player_number - 1] = 16;

			//유저가 시작기준 인원만큼 들어왔으면 게임 시작
			if (GameInfo[i]->player_count == PLAYER_COUNT)
			{
				GameInfo[i]->full = true;
				GameInfo[i]->state = GAME_STATE::G_PLAYING_STATE;
			}

			game_ptr = GameInfo[i];
			index = i;
			break;
		}
	}

	//비어있는 게임방이 없다면 게임방 생성하고 유저정보 추가
	if (index == -1)
	{
		game_ptr = new _GameInfo;
		ZeroMemory(game_ptr, sizeof(_GameInfo));
		game_ptr->full = false;		
		game_ptr->players[0] = _ptr;

		_ptr->game_info = game_ptr;
		_ptr->player_number = 1;
		SetCards(game_ptr->Cards[0]);
		game_ptr->remain[0] = 16;

		game_ptr->player_count++;
		game_ptr->state = GAME_STATE::G_WAIT_STATE;
		GameInfo[GameCount++] = game_ptr;
	}

	LeaveCriticalSection(&cs);
	return game_ptr;
}

//게임방 제거
void ReMoveGameInfo(_GameInfo* _ptr)
{
	EnterCriticalSection(&cs);
	for (int i = 0; i < GameCount; i++)
	{
		if (GameInfo[i] == _ptr)
		{			
			delete _ptr;
			for (int j = i; j < GameCount - 1; j++)
			{
				GameInfo[j] = GameInfo[j + 1];
			}
			break;
		}
	}

	GameCount--;
	LeaveCriticalSection(&cs);
}