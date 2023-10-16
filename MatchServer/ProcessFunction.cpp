#include "Common.h"

void RecvPacketProcess(_ClientInfo* _ptr)
{	
	PROTOCOL protocol = GetProtocol(_ptr->recvbuf);

	//PlayProcess에 승패판정이 들어가있기 때문에 판정 도중 상대도 끝날경우 판정이 꼬일수 있음
	//그런 상황을 막기 위해 임계영역 설정
	EnterCriticalSection(&cs);
	switch (_ptr->state)
	{	
		//게임 진행
	case CLIENT_STATE::C_PLAYING_STATE:
		PlayProcess(_ptr);
		break;
	}
	LeaveCriticalSection(&cs);
}

void SendPacketProcess(_ClientInfo* _ptr)
{	
	_GameInfo* game_info = _ptr->game_info;
	_ClientInfo* next_ptr = nullptr;
	int size;
	char buf[BUFSIZE];

	switch (_ptr->state)
	{
	case CLIENT_STATE::C_INIT_STATE:
		switch (game_info->state)
		{
			//게임 시작메세지 발송 완료하면 플레이상태로 변경
		case GAME_STATE::G_PLAYING_STATE:
			_ptr->state = CLIENT_STATE::C_PLAYING_STATE;
			break;
		}				
		break;
	case CLIENT_STATE::C_PLAYING_STATE:
		switch (game_info->state)
		{
		case GAME_STATE::G_GAME_OVER_STATE:
			_ptr->state = CLIENT_STATE::C_GAME_OVER_STATE;
			break;
		case GAME_STATE::G_PLAYING_STATE:
			break;
		case GAME_STATE::G_INVALID_GAME_STATE:
			_ptr->state = CLIENT_STATE::C_GAME_OVER_STATE;
			break;
		}
		break;
	case CLIENT_STATE::C_GAME_OVER_STATE:
		break;
	}
}


void MakeGameMessage(_ClientInfo* _ptr, int _count, int* _data, char* _msg)
{
	char temp[3] = { 0 };

	_itoa(_ptr->player_number, temp, 10);
	strcpy(_msg, temp);
	strcat(_msg, "번 Player가 선택한 숫자는");

	for (int i = 0; i < _count; i++)
	{
		_itoa(_data[i], temp, 10);
		strcat(_msg, temp);
		strcat(_msg, "  ");
	}

	strcat(_msg, "입니다.\n");
}

//게임 진행을 담당
void PlayProcess(_ClientInfo* _ptr)
{
	_GameInfo* game = _ptr->game_info;
	char buf[BUFSIZE];
	int x, y, tx, ty, size;
	bool flag = false;

	PROTOCOL protocol = GetProtocol(_ptr->recvbuf);

	switch (protocol)
	{
	case PROTOCOL::CLICK_POSITION:
		//클릭 위치 받아서 처리하고 두 클라에 결과 send
		UnPackPacket(_ptr->recvbuf, x, y);

		//임시로 뒤집어둔 카드가 있다면 어떤 칸에 있는지 받아옴 없으면 -1저장
		GetTempFlip(&tx, &ty, game->Cards[_ptr->player_number - 1]);
		//임시로 뒤집어둔 카드가 없으면 이번 카드를 임시카드로 설정
		if (tx == -1)
		{
			game->Cards[_ptr->player_number - 1][x][y].State = TEMPFLIP;
		}
		else
		{
			//임시로 뒤집어둔 카드가 있으면 임시카드와 이번 카드의 그림이 맞는지 확인
			if (game->Cards[_ptr->player_number - 1][tx][ty].Num == game->Cards[_ptr->player_number - 1][x][y].Num)
			{
				//맞으면 둘다 뒤집어진 상태로 유지
				game->Cards[_ptr->player_number - 1][tx][ty].State = FLIP;
				game->Cards[_ptr->player_number - 1][x][y].State = FLIP;
				//남은 카드수 감소
				game->remain[_ptr->player_number - 1] -= 2;

				//남은 카드가 없으면
				if (game->remain[_ptr->player_number - 1] == 0)
				{
					flag = true;
				}
			}
			else
			{
				//틀리면 이번 카드도 임시카드로 설정
				game->Cards[_ptr->player_number - 1][x][y].State = TEMPFLIP;
			}
		}

		//게임에 참여한 모든 인원에게 클릭 결과 메세지 전송
		for (int i = 0; i < game->player_count; i++)
		{
			if (i == _ptr->player_number - 1)
			{
				//본인에게 카드판정보 전송
				size = PackPacket(buf, PROTOCOL::MY_CLICK, game->Cards[_ptr->player_number - 1], game->remain[_ptr->player_number - 1]);
				Send(game->players[i], buf, size);
			}
			else
			{
				//상대에게 카드판정보 전송
				size = PackPacket(buf, PROTOCOL::OPPONENET_CLICK, game->Cards[_ptr->player_number - 1], game->remain[_ptr->player_number - 1]);
				Send(game->players[i], buf, size);
			}
		}

		if (flag) {
			//게임에 참여한 모든 인원 게임종료 상태로 전환
			for (int i = 0; i < game->player_count; i++)
			{
				game->players[i]->state = CLIENT_STATE::C_GAME_OVER_STATE;

				if (i + 1 == _ptr->player_number)
				{
					//승리메세지 전송
					size = PackPacket(buf, PROTOCOL::RESULT, WIN_MSG);
					Send(_ptr, buf, size);
				}
				else
				{
					//패배메세지 전송
					size = PackPacket(buf, PROTOCOL::RESULT, LOSE_MSG);
					Send(game->players[i], buf, size);
				}
			}
			flag = false;
		}
		break;
	case HIDE_TEMP_CARD:
		//임시공개 카드들 타이머 끝나서 신호 받으면 다시 가림
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				if (game->Cards[_ptr->player_number - 1][i][j].State == TEMPFLIP)
					game->Cards[_ptr->player_number - 1][i][j].State = HIDDEN;
			}
		}

		//이 경우 본인 카드는 이미 가린 상태이므로 상대에게만 전송
		for (int i = 0; i < game->player_count; i++)
		{
			if (i != _ptr->player_number - 1)
			{
				//상대에게 카드판정보 전송
				size = PackPacket(buf, PROTOCOL::OPPONENET_CLICK, game->Cards[_ptr->player_number - 1], game->remain[_ptr->player_number - 1]);
				Send(game->players[i], buf, size);
			}
		}
		break;
	}
}

//클라이언트의 강제종료로 인한 게임오버 처리
void InvalidGameProcess(_ClientInfo* _ptr)
{
	int size;
	int retval;
	char buf[BUFSIZE];
	_GameInfo* game = _ptr->game_info;

	if (game->state == GAME_STATE::G_INVALID_GAME_STATE)
	{
		char msg[BUFSIZE] = { 0 };
		strcpy(msg, "함께할 Player가 하나도 없어서 게임을 종료합니다.\n");

		size = PackPacket(buf, PROTOCOL::INVALID_GAME, msg);
		Send(_ptr, buf, size);		
		return;
	}
}

void InitProcess(_ClientInfo* _ptr)
{
	_GameInfo* game_info = _AddGameInfo(_ptr);
	int retval, size;
	char msg[BUFSIZE], buf[BUFSIZE];

	switch (game_info->state)
	{
	case GAME_STATE::G_WAIT_STATE:
		//대기상태라면 대기메세지 발송
		size = PackPacket(buf, PROTOCOL::WAIT, WAIT_MSG);
		Send(_ptr, buf, size);
		break;
	case GAME_STATE::G_PLAYING_STATE:	
		//게임 시작하면 시작메세지와 각자의 카드판 정보 발송
		for (int i = 0; i < game_info->player_count; i++)
		{
			size = PackPacket(buf, PROTOCOL::INTRO, INTRO_MSG, game_info->Cards[i]);
			Send(game_info->players[i], buf, size);
		}
	}

}

void UpdateGameInfo(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_GameInfo* game = _ptr->game_info;

	for (int i = 0; i < game->player_count; i++)
	{
		if (game->players[i] == _ptr)
		{
			for (int j = i; j < game->player_count - 1; j++)
			{
				game->players[j] = game->players[j + 1];
			}
			game->player_count--;

			if (game->state==GAME_STATE::G_PLAYING_STATE && game->player_count == 1)
			{
				game->state = GAME_STATE::G_INVALID_GAME_STATE;
				InvalidGameProcess(game->players[0]);				
			}

			if (game->player_count == 0)
			{
				ReMoveGameInfo(game);
			}
			break;
		}

	}
	LeaveCriticalSection(&cs);
}

_ClientInfo* SearchNextClient(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);

	_GameInfo* game = _ptr->game_info;

	if (game->player_count == 1)
	{
		LeaveCriticalSection(&cs);
		return nullptr;
	}

	int i = -1;
	for (i = 0; i < game->player_count; i++)
	{
		if (game->players[i] == _ptr)
		{
			break;
		}
	}

	if (i == -1)
	{
		return nullptr;
	}

	int index = (i + 1) % game->player_count;
	_ClientInfo* ptr = game->players[index];

	LeaveCriticalSection(&cs);
	return ptr;
}

void ExitClientCheck(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_GameInfo* game = _ptr->game_info;
	int size;
	char buf[BUFSIZE];

	switch (game->state)
	{
	case G_WAIT_STATE:
		break;
	case G_PLAYING_STATE:
	{
		char msg[BUFSIZE] = { 0 };
		sprintf(msg, "%d번 Player가 나갔습니다. \n", _ptr->player_number);

		for (int i = 0; i < game->player_count; i++)
		{
			_ClientInfo* tempptr = game->players[i];

			size = PackPacket(buf, PROTOCOL::PLAYER_INFO, msg);
			Send(tempptr, buf, size);
		}
		break;

	}
	break;
	case G_GAME_OVER_STATE:
	case G_INVALID_GAME_STATE:
		break;
	}

	LeaveCriticalSection(&cs);
}

void SetCards(CARD_INFO cards[4][4])
{
	int i, j;
	int x, y;

	memset(cards, 0, sizeof(cards));

	srand(GetTickCount());

	//i는 그림이 들어간 배열의 칸 번호
	//j는 2개씩 짝으로 들어가야 한다는 뜻
	for (i = 1; i <= 8; i++)
	{
		for (j = 0; j < 2; j++)
		{
			do {
				x = rand() % 4;
				y = rand() % 4;
			} while (cards[x][y].Num != 0);
			cards[x][y].Num = i;
			cards[x][y].State = HIDDEN;
		}
	}
}

void GetTempFlip(int* tx, int* ty, CARD_INFO cards[4][4])
{
	int i, j;
	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			if (cards[i][j].State == TEMPFLIP)
			{
				*tx = i;
				*ty = j;
				return;
			}
		}
	}
	*tx = -1;
}

void DisconnectedProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	ExitClientCheck(_ptr);
	UpdateGameInfo(_ptr);
	RemoveClientInfo(_ptr);
	LeaveCriticalSection(&cs);
}
