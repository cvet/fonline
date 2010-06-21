#include "StdAfx.h"
#include "net.h"

z_stream net_zstrm;							// Структура для паковки
SOCKET net_sockfd = INVALID_SOCKET;			// Присоединенный сокет
bool zlib_init = false;
char * ComBuf = NULL;
UINT comlen = 0;

fd_set fd_write, fd_read, fd_exc;

bool Output(CBufMngr & buf)
{
	if (net_sockfd == INVALID_SOCKET)
	{
		printLog("Network system was not initialized\n");	
		return false;
	}

	if(!buf.pos) return 1;

	int tosend = buf.pos;
	int sendpos=0;
	while(sendpos<tosend)
	{
		int bsent=send(net_sockfd,buf.data+sendpos,tosend-sendpos,0);
		sendpos+=bsent;
		if(bsent==SOCKET_ERROR)
		{
			WriteLog("SOCKET_ERROR while send forSockID=%d\n",net_sockfd);
			return false;
		}
	}

	WriteLog("Серверу было послано %d байт\n", buf.pos);
	buf.reset();
	return true;
}

bool Input(CBufMngr & buf)
{
	if (net_sockfd == INVALID_SOCKET)
	{
		printLog("Network system is not init. \n");
		return false;
	}

	UINT len=recv(net_sockfd, buf.data, buf.len,0);
	if(len==SOCKET_ERROR || !len) 
	{
		printLog("Input error\n");
		return false;
	}

	buf.pos = len;
	while( buf.pos == buf.len )
	{
		buf.grow_buf(buf.len << 1);
		len = recv(net_sockfd, buf.data + buf.pos, buf.len - buf.pos,0);
		buf.pos += len;
	}

	WriteLog("От сервера получено %d байт \n", buf.pos);

	return 1;
}

bool InputCompressed(CBufMngr & buf)
{
	buf.reset();

	UINT len=recv(net_sockfd,ComBuf,comlen,0);
	if(len==SOCKET_ERROR || !len) 
	{
		//ErrMsg("CFEngine::NetInput","Socket error!\r\n");
		printLog("Socket error!\n");
		return false;
	}
	bool rebuf=0;
	
	UINT compos=len;
	
	while(compos==comlen)
	{
		rebuf=1;
		DWORD newcomlen=comlen<<1;
		char * NewCOM=new char[newcomlen];
		memcpy(NewCOM,ComBuf,comlen);
		SAFEDELA(ComBuf);
		ComBuf=NewCOM;
		comlen=newcomlen;
		
		len=recv(net_sockfd,ComBuf+compos,comlen-compos,0);
		compos+=len;
	}
	
	if(rebuf) buf.grow_buf(comlen<<1);
	buf.reset();

	net_zstrm.next_in=(UCHAR*)ComBuf;
	net_zstrm.avail_in=compos;
	net_zstrm.next_out=(UCHAR*)buf.data;
	net_zstrm.avail_out=buf.len;

	if(inflate(&net_zstrm,Z_SYNC_FLUSH)!=Z_OK)
	{
		printLog("Inflate error!\r\n");
		return false;
	}
	
	buf.pos=net_zstrm.next_out-(UCHAR*)buf.data;
	
	while(net_zstrm.avail_in)
	{
		buf.grow_buf(buf.len << 1);
		// !Deniska
		WriteLog("Недостаточно места во входном буфере. Размер буфера увеличен до %d\n", buf.len);
		
		net_zstrm.next_out=(UCHAR*)buf.data+buf.pos;
		net_zstrm.avail_out=buf.len-buf.pos;
			
		if(inflate(&net_zstrm,Z_SYNC_FLUSH)!=Z_OK)
		{
			printLog("Inflate continue error!\n");
			return false;
		}
		buf.pos=net_zstrm.next_out-(UCHAR*)buf.data;
	}
	WriteLog("\nrecv %d->%d bytes\n",compos,buf.pos);
	
	return true;
}

void Disconnect(void)
{
	inflateReset(&net_zstrm);
	if (net_sockfd == INVALID_SOCKET) return;
	closesocket(net_sockfd);
	net_sockfd = INVALID_SOCKET;
}

int Connect(char * host, WORD port)
{
	if (net_sockfd == INVALID_SOCKET)
	{
		WriteLog("Сетевой интерфейс не был инициализирован.\n");
		return NET_ERR_NOT_INIT;
	}

	printLog("Connect to server: %s:%d\n", host, port);	
	sockaddr_in servaddr;
	
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	if ( (servaddr.sin_addr.s_addr = inet_addr(host)) == -1)		
	{
		printLog("Cannot resolve remote host %s \n", host);
		return NET_ERR_INVALID_ADDRESS;
	}	
	
	// Пытаемся присоединиться.
	int res = connect(net_sockfd, (sockaddr*)&servaddr, sizeof(servaddr));
	if (res)
	{
		printLog("Can't connect to the server.........\n");
		return NET_ERR_SERVER_IS_DOWN;
	}
	
	printLog("Connection with the server was established\n");
	
	return NET_OK;
}

void FinishNet(void)
{
	SAFEDELA(ComBuf);
	inflateEnd(&net_zstrm);
	zlib_init = false;
	if (net_sockfd == INVALID_SOCKET) return;
	closesocket(net_sockfd);
	net_sockfd = INVALID_SOCKET;
}

SOCKET InitNet(void)
{
	WriteLog("Инициализация сокетов\n");
	
	WORD	wVersionRequested;
	WSADATA wsaData;
	int		err;
	
	wVersionRequested = MAKEWORD( 2, 2 );
	
	err = WSAStartup( wVersionRequested, &wsaData );
	if (err != 0)
	{
		printLog("Функция WSAStartup вернула код ошибки %d\n", err);
		return INVALID_SOCKET;
	}
	
	net_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (net_sockfd == INVALID_SOCKET) 
	{
		WriteLog("Не могу инициализировать клиентский сокет\n");
		return INVALID_SOCKET;
	}
	
	if ( LOBYTE( wsaData.wVersion ) != 2 || HIBYTE( wsaData.wVersion ) != 2 ) 
	{
		printLog("Версия сокетов 2.2 не поддерживается данной реализацией Windows Socket\n");
		WSACleanup( );
		return INVALID_SOCKET;
	}	
	
	if (!zlib_init)
	{
		WriteLog("Инициализация модуля zlib\n");
		net_zstrm.zalloc = zlib_alloc;
		net_zstrm.zfree = zlib_free;
		net_zstrm.opaque = NULL;
		
		if(inflateInit(&net_zstrm)!=Z_OK)
		{
			printLog("InflateInit error!\n");
			return INVALID_SOCKET;
		}
		WriteLog("Инициализация модуля zlib завершена успешно\n");
		zlib_init = true;
	}

	// Ждем сигналов.
	FD_ZERO(&fd_read);
	FD_ZERO(&fd_exc);
	FD_ZERO(&fd_write);

	ComBuf = new char[BUF_SIZE];
	comlen = BUF_SIZE;

	WriteLog("Инициализация сокетов завершена успешно. socket = %d\n", net_sockfd);
	
	return net_sockfd;
}

void CheckNet(timeval tv)
{
	if (net_sockfd == INVALID_SOCKET) return;

	// Ждем сигналов.
	FD_ZERO(&fd_read);
	FD_ZERO(&fd_exc);
	FD_ZERO(&fd_write);

	FD_SET(net_sockfd, &fd_read);
	FD_SET(net_sockfd, &fd_write);
	
	// Функция select возвращается немендленно(не ждет).
	select(0,&fd_read,&fd_write,&fd_exc,&tv);
}

int ISSetRead(void)
{
	return FD_ISSET(net_sockfd, &fd_read);
}

int ISSetWrite(void)
{
	return FD_ISSET(net_sockfd, &fd_write);
}

int ISSetExc(void)
{
	return FD_ISSET(net_sockfd, &fd_exc);
}
