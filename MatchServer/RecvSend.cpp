#include "Common.h"
bool Recv(_ClientInfo* _ptr)
{
	int retval;
	DWORD recvbytes;
	DWORD flag = 0;

	ZeroMemory(&_ptr->r_overlapped.overlapped, sizeof(WSAOVERLAPPED));

	_ptr->r_wsabuf.buf = _ptr->recvbuf + _ptr->comp_recvbytes;
	_ptr->r_wsabuf.len = _ptr->recvbytes - _ptr->comp_recvbytes;

	retval = WSARecv(_ptr->sock, &_ptr->r_wsabuf, 1, &recvbytes, &flag, &_ptr->r_overlapped.overlapped, NULL);
	if (retval == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			err_display("WSARecv");
			return false;
		}
	}

	return true;
}

int CompleteRecv(_ClientInfo* _ptr, int _cbTransferred)
{
	_ptr->comp_recvbytes += _cbTransferred;

	if (_ptr->comp_recvbytes == _ptr->recvbytes)
	{
		if (!_ptr->r_sizeflag)
		{
			memcpy(&_ptr->recvbytes, _ptr->recvbuf, sizeof(int));
			_ptr->comp_recvbytes = 0;
			_ptr->r_sizeflag = true;
		}
		else
		{
			_ptr->comp_recvbytes = 0;
			_ptr->r_sizeflag = false;
			_ptr->recvbytes = sizeof(int);
			return SOC_TRUE;
		}
	}

	if (!Recv(_ptr))
	{
		RemoveClientInfo(_ptr);
		return SOC_ERROR;
	}

	return SOC_FALSE;
}


bool Send(_ClientInfo* _ptr, char* _buf=nullptr, int size=0)
{
	int retval;
	DWORD sendbytes;
	DWORD flag = 0;
	_SendPacket* packet = nullptr;

	if (_buf != nullptr && size != 0)
	{
		_SendPacket* packet = new _SendPacket();
		ZeroMemory(packet, sizeof(_SendPacket));
		memcpy(packet->sendbuf, _buf, size);
		packet->sendbytes = size;
		packet->s_overlapped.ptr = _ptr;
		packet->s_overlapped.type = IO_TYPE::SEND;

		_ptr->SendQueue->push(packet);
		if (_ptr->SendQueue->size() > 1)
		{
			return true;
		}
	}

	packet = _ptr->SendQueue->front();

	ZeroMemory(&packet->s_overlapped.overlapped, sizeof(WSAOVERLAPPED));

	packet->s_wsabuf.buf = packet->sendbuf + packet->comp_sendbytes;
	packet->s_wsabuf.len = packet->sendbytes - packet->comp_sendbytes;

	retval = WSASend(_ptr->sock, &packet->s_wsabuf, 1, &sendbytes, 0, &packet->s_overlapped.overlapped, NULL);
	if (retval == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			//printf("%d %d %d\n", _ptr->game_result, _ptr->player_number, _ptr->sendbytes);
			err_display("WSASend()");
			return false;
		}
	}

	return true;
}

int CompleteSend(_ClientInfo* _ptr, int _cbTransferred)
{
	_SendPacket* packet = _ptr->SendQueue->front();

	packet->comp_sendbytes += _cbTransferred;

	if (packet->comp_sendbytes == packet->sendbytes)
	{
		_ptr->SendQueue->pop();
		delete packet;

		if (_ptr->SendQueue->size() > 0)
		{
			Send(_ptr);
		}

		return SOC_TRUE;
	}

	if (!Send(_ptr))
	{
		return SOC_ERROR;
	}

	return SOC_FALSE;
}

