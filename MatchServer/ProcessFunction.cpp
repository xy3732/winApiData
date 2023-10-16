#include "Common.h"

void RecvPacketProcess(_ClientInfo* _ptr)
{	
	PROTOCOL protocol = GetProtocol(_ptr->recvbuf);

	//PlayProcess�� ���������� ���ֱ� ������ ���� ���� ��뵵 ������� ������ ���ϼ� ����
	//�׷� ��Ȳ�� ���� ���� �Ӱ迵�� ����
	EnterCriticalSection(&cs);
	switch (_ptr->state)
	{	
		//���� ����
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
			//���� ���۸޼��� �߼� �Ϸ��ϸ� �÷��̻��·� ����
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
	strcat(_msg, "�� Player�� ������ ���ڴ�");

	for (int i = 0; i < _count; i++)
	{
		_itoa(_data[i], temp, 10);
		strcat(_msg, temp);
		strcat(_msg, "  ");
	}

	strcat(_msg, "�Դϴ�.\n");
}

//���� ������ ���
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
		//Ŭ�� ��ġ �޾Ƽ� ó���ϰ� �� Ŭ�� ��� send
		UnPackPacket(_ptr->recvbuf, x, y);

		//�ӽ÷� ������� ī�尡 �ִٸ� � ĭ�� �ִ��� �޾ƿ� ������ -1����
		GetTempFlip(&tx, &ty, game->Cards[_ptr->player_number - 1]);
		//�ӽ÷� ������� ī�尡 ������ �̹� ī�带 �ӽ�ī��� ����
		if (tx == -1)
		{
			game->Cards[_ptr->player_number - 1][x][y].State = TEMPFLIP;
		}
		else
		{
			//�ӽ÷� ������� ī�尡 ������ �ӽ�ī��� �̹� ī���� �׸��� �´��� Ȯ��
			if (game->Cards[_ptr->player_number - 1][tx][ty].Num == game->Cards[_ptr->player_number - 1][x][y].Num)
			{
				//������ �Ѵ� �������� ���·� ����
				game->Cards[_ptr->player_number - 1][tx][ty].State = FLIP;
				game->Cards[_ptr->player_number - 1][x][y].State = FLIP;
				//���� ī��� ����
				game->remain[_ptr->player_number - 1] -= 2;

				//���� ī�尡 ������
				if (game->remain[_ptr->player_number - 1] == 0)
				{
					flag = true;
				}
			}
			else
			{
				//Ʋ���� �̹� ī�嵵 �ӽ�ī��� ����
				game->Cards[_ptr->player_number - 1][x][y].State = TEMPFLIP;
			}
		}

		//���ӿ� ������ ��� �ο����� Ŭ�� ��� �޼��� ����
		for (int i = 0; i < game->player_count; i++)
		{
			if (i == _ptr->player_number - 1)
			{
				//���ο��� ī�������� ����
				size = PackPacket(buf, PROTOCOL::MY_CLICK, game->Cards[_ptr->player_number - 1], game->remain[_ptr->player_number - 1]);
				Send(game->players[i], buf, size);
			}
			else
			{
				//��뿡�� ī�������� ����
				size = PackPacket(buf, PROTOCOL::OPPONENET_CLICK, game->Cards[_ptr->player_number - 1], game->remain[_ptr->player_number - 1]);
				Send(game->players[i], buf, size);
			}
		}

		if (flag) {
			//���ӿ� ������ ��� �ο� �������� ���·� ��ȯ
			for (int i = 0; i < game->player_count; i++)
			{
				game->players[i]->state = CLIENT_STATE::C_GAME_OVER_STATE;

				if (i + 1 == _ptr->player_number)
				{
					//�¸��޼��� ����
					size = PackPacket(buf, PROTOCOL::RESULT, WIN_MSG);
					Send(_ptr, buf, size);
				}
				else
				{
					//�й�޼��� ����
					size = PackPacket(buf, PROTOCOL::RESULT, LOSE_MSG);
					Send(game->players[i], buf, size);
				}
			}
			flag = false;
		}
		break;
	case HIDE_TEMP_CARD:
		//�ӽð��� ī��� Ÿ�̸� ������ ��ȣ ������ �ٽ� ����
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				if (game->Cards[_ptr->player_number - 1][i][j].State == TEMPFLIP)
					game->Cards[_ptr->player_number - 1][i][j].State = HIDDEN;
			}
		}

		//�� ��� ���� ī��� �̹� ���� �����̹Ƿ� ��뿡�Ը� ����
		for (int i = 0; i < game->player_count; i++)
		{
			if (i != _ptr->player_number - 1)
			{
				//��뿡�� ī�������� ����
				size = PackPacket(buf, PROTOCOL::OPPONENET_CLICK, game->Cards[_ptr->player_number - 1], game->remain[_ptr->player_number - 1]);
				Send(game->players[i], buf, size);
			}
		}
		break;
	}
}

//Ŭ���̾�Ʈ�� ��������� ���� ���ӿ��� ó��
void InvalidGameProcess(_ClientInfo* _ptr)
{
	int size;
	int retval;
	char buf[BUFSIZE];
	_GameInfo* game = _ptr->game_info;

	if (game->state == GAME_STATE::G_INVALID_GAME_STATE)
	{
		char msg[BUFSIZE] = { 0 };
		strcpy(msg, "�Բ��� Player�� �ϳ��� ��� ������ �����մϴ�.\n");

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
		//�����¶�� ���޼��� �߼�
		size = PackPacket(buf, PROTOCOL::WAIT, WAIT_MSG);
		Send(_ptr, buf, size);
		break;
	case GAME_STATE::G_PLAYING_STATE:	
		//���� �����ϸ� ���۸޼����� ������ ī���� ���� �߼�
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
		sprintf(msg, "%d�� Player�� �������ϴ�. \n", _ptr->player_number);

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

	//i�� �׸��� �� �迭�� ĭ ��ȣ
	//j�� 2���� ¦���� ���� �Ѵٴ� ��
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
