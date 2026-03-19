#include "scriptsocket.h"
#include <assert.h>
#include "../autowrapper/aswrappedcall.h"

BEGIN_AS_NAMESPACE

// For now, this is only supported on Windows
#ifdef _WIN32



#ifdef _WIN32

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

// Manage the required calls to WSAStartup and WSACleanup
class WindowsSocketLib
{
public:
	WindowsSocketLib() : m_status(0)
	{
		WORD wVersionRequested = 0x0202;
		WSADATA wsadata;
		// WSAStartup can be called multiple times, so it is not a problem if another piece of code also called it
		m_status = WSAStartup(wVersionRequested, &wsadata);
		if (m_status != 0)
		{
			// No usable WINSOCK.DLL found
		}
		else if (wsadata.wVersion != wVersionRequested)
		{
			// WINSOCK.DLL does not support version 2.2
			WSACleanup();
			m_status = -1;
		}
	}

	~WindowsSocketLib()
	{
		// WSACleanup must be called for each successful call to WSAStartup
		if( m_status == 0 )
			WSACleanup();
	}

	int m_status;
} g_windowsSocketLib;
#endif

CScriptSocket::CScriptSocket() : m_refCount(1), m_socket(-1), m_isListening(false)
{
	// TODO: On Windows check if the Windows Socket was properly loaded, else raise a script exception
}

void CScriptSocket::AddRef() const
{
	asAtomicInc(m_refCount);;
}

void CScriptSocket::Release() const
{
	if (asAtomicDec(m_refCount) == 0)
		delete this;
}

CScriptSocket::~CScriptSocket()
{
	// Disconnect the socket if it is open
	Close();
}

int CScriptSocket::Listen(asWORD port)
{
	// If another socket is already used it must first be closed, and the working thread must be shutdown
	if (m_socket != -1)
		return -1;

	// Set up a listener socket
	// TODO: Allow script to define the protocol
	m_socket = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_socket == -1)
		return -1;

	sockaddr_in serverAddress = { 0 };
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(port);
	serverAddress.sin_addr.s_addr = INADDR_ANY;

	// TODO: Need to be able to tell if the port is already occupied
	int r = bind(m_socket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
	if (r == SOCKET_ERROR)
	{
		closesocket(m_socket);
		m_socket = -1;
		return -1;
	}
	m_isListening = true;

	// TODO: Allow script to define the max queue for incoming connections
	listen(m_socket, 25);

	return 0;
}

// Internal
// Returns 1 if there is something to read on the socket
// Returns 0 if there is nothing to read
// Returns a negative value if there is an error (see return codes for select())
int CScriptSocket::Select(asINT64 timeoutMicrosec)
{
	if (!IsActive())
		return -1;

	fd_set read = { 1, {(SOCKET)m_socket} };
	TIMEVAL timeout = { 0, 0 }; // Don't wait
	if (timeoutMicrosec > 0)
	{
		// Limit the wait time to the highest value that TIMEVAL can represent,
		// i.e. 2^31-1 seconds + 999,999 microseconds (approximately 68 years)
		const asINT64 maxTimeout = asINT64(0x7FFFFFFF) * 1000000 + 999999;
		if (timeoutMicrosec > maxTimeout) timeoutMicrosec = maxTimeout;
		timeout.tv_sec = asINT32(timeoutMicrosec / 1000000) & 0x7FFFFFFF;
		timeout.tv_usec = asINT32(timeoutMicrosec - asINT64(timeout.tv_sec) * 1000000);
	}

	int r = select(0, &read, 0, 0, timeoutMicrosec < 0 ? 0 : &timeout);

	// If any error occurred then close the socket
	// TODO: Use WSAGetLastError to determine exact error
	if (r < 0)
		Close();

	return r;
}

CScriptSocket* CScriptSocket::Accept(asINT64 timeoutMicrosec)
{
	// Cannot accept a client on an ordinary socket or if the socket is not active
	if (!m_isListening || m_socket == -1)
		return 0;

	// First determine if there is anything to receive so that doesn't block
	int r = Select(timeoutMicrosec);
	if (r <= 0)
		return 0;

	// TODO: need to be able to check to what ip address and port the socket is connected it (use getsockopt with SO_BSP_STATE)
	// For each incoming client connection a new CScriptSocket is created
	int clientSocket = (int)accept(m_socket, 0, 0);
	if (clientSocket != -1)
	{
		CScriptSocket* client = new CScriptSocket();
		client->m_socket = clientSocket;
		return client;
	}

	return 0;
}

int CScriptSocket::Close()
{
	// If the socket is open
	if (m_socket == -1)
		return -1;

	// Close the listener socket
	closesocket(m_socket);
	m_socket = -1;

	return 0;
}

int CScriptSocket::Connect(asUINT ipv4Address, asWORD port)
{
	// If another socket is already used it must first be closed, and the working thread must be shutdown
	if (m_socket != -1)
		return -1;

	// Set up a client socket
	// TODO: Allow script to define the protocol
	m_socket = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	sockaddr_in serverAddress = { 0 };
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(port);
	serverAddress.sin_addr.s_addr = htonl(ipv4Address);

	// TODO: Allow script to define a timeout
	int result = connect(m_socket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
	if (result == SOCKET_ERROR)
	{
		closesocket(m_socket);
		m_socket = -1;
		return -1;
	}

	return 0;
}

int CScriptSocket::Send(const std::string& data)
{
	// Cannot send on a listener socket or if the socket is not connected
	if (m_isListening || m_socket == -1)
		return -1;

	// Send a buffer of data over the socket
	// TODO: The sending of data should be done in a background thread so it can be done over a time rather than block the caller
	// TODO: How to determine the maximum size of data that can be sent in a single call?
	int r = send(m_socket, data.c_str(), (int)data.length(), 0);
	if (r == SOCKET_ERROR)
	{
		// TODO: Use WSAGetLastError to determine the type of the error

		// If an error happens, then we close the socket
		Close();
		return -1;
	}

	// Return the number of bytes sent
	return r;
}

std::string CScriptSocket::Receive(asINT64 timeoutMicrosec)
{
	// Cannot receive on a listener socket or if the socket is not connected
	if (m_isListening || m_socket == -1)
		return "";

	// TODO: Need to be able to set the size of the internal buffer
	
	// First determine if there is anything to receive so that doesn't block
	int r = Select(timeoutMicrosec);
	if (r <= 0)
		return "";

	char buf[1024] = {};
	std::string msg;
	for (;;)
	{
		// Read the buffer
		r = recv(m_socket, buf, sizeof(buf), 0);
		if (r == 0)
		{
			// The socket is closed from the other side
			Close();
			break;
		}
		else if (r >= 0)
		{
			msg.append(buf, r);
			break;
		}
		else if (r == SOCKET_ERROR)
		{
			r = WSAGetLastError();
			if (r == WSAEMSGSIZE)
			{
				// Append what could be read, then read the rest
				msg.append(buf, sizeof(buf));
			}
			else
			{
				// For any other error we just close the socket
				Close();
				break;
			}
		}
	}

	return msg;
}

bool CScriptSocket::IsActive() const
{
	if (m_socket == -1) 
		return false;

	int error_code = 0;
	int error_code_size = sizeof(error_code);
	int r = getsockopt(m_socket, SOL_SOCKET, SO_ERROR, (char*)&error_code, &error_code_size);
	if (r < 0 || error_code != 0)
	{
		// If an error occurred just close the socket
		const_cast<CScriptSocket*>(this)->Close();
		return false;
	}

	return true;
}

static CScriptSocket* CScriptSocket_Factory()
{
	return new CScriptSocket();
}

int RegisterScriptSocket_Native(asIScriptEngine* engine)
{
	int r; 

	// Check that the string type has been registered already
	r = engine->GetTypeIdByDecl("string"); assert(r >= 0);

	// Register the socket class with the script engine
	engine->RegisterObjectType("socket", 0, asOBJ_REF);
	r = engine->RegisterObjectBehaviour("socket", asBEHAVE_FACTORY, "socket @f()", asFUNCTION(CScriptSocket_Factory), asCALL_CDECL); assert(r >= 0);
	r = engine->RegisterObjectBehaviour("socket", asBEHAVE_ADDREF, "void f()", asMETHOD(CScriptSocket, AddRef), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectBehaviour("socket", asBEHAVE_RELEASE, "void f()", asMETHOD(CScriptSocket, Release), asCALL_THISCALL); assert(r >= 0);

	r = engine->RegisterObjectMethod("socket", "int listen(uint16 port)", asMETHOD(CScriptSocket, Listen), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("socket", "int close()", asMETHOD(CScriptSocket, Close), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("socket", "socket @accept(int64 timeout = 0)", asMETHOD(CScriptSocket, Accept), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("socket", "int connect(uint ipv4address, uint16 port)", asMETHOD(CScriptSocket, Connect), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("socket", "int send(const string &in data)", asMETHOD(CScriptSocket, Send), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("socket", "string receive(int64 timeout = 0)", asMETHOD(CScriptSocket, Receive), asCALL_THISCALL); assert(r >= 0);
	r = engine->RegisterObjectMethod("socket", "bool isActive() const", asMETHOD(CScriptSocket, IsActive), asCALL_THISCALL); assert(r >= 0);

	return 0;
}

int RegisterScriptSocket_Generic(asIScriptEngine* engine)
{
	int r;

	// Check that the string type has been registered already
	r = engine->GetTypeIdByDecl("string"); assert(r >= 0);

	// Register the socket class with the script engine
	engine->RegisterObjectType("socket", 0, asOBJ_REF);
	r = engine->RegisterObjectBehaviour("socket", asBEHAVE_FACTORY, "socket @f()", WRAP_FN(CScriptSocket_Factory), asCALL_GENERIC); assert(r >= 0);
	r = engine->RegisterObjectBehaviour("socket", asBEHAVE_ADDREF, "void f()", WRAP_MFN(CScriptSocket, AddRef), asCALL_GENERIC); assert(r >= 0);
	r = engine->RegisterObjectBehaviour("socket", asBEHAVE_RELEASE, "void f()", WRAP_MFN(CScriptSocket, Release), asCALL_GENERIC); assert(r >= 0);

	r = engine->RegisterObjectMethod("socket", "int listen(uint16 port)", WRAP_MFN(CScriptSocket, Listen), asCALL_GENERIC); assert(r >= 0);
	r = engine->RegisterObjectMethod("socket", "int close()", WRAP_MFN(CScriptSocket, Close), asCALL_GENERIC); assert(r >= 0);
	r = engine->RegisterObjectMethod("socket", "socket @accept(int64 timeout = 0)", WRAP_MFN(CScriptSocket, Accept), asCALL_GENERIC); assert(r >= 0);
	r = engine->RegisterObjectMethod("socket", "int connect(uint ipv4address, uint16 port)", WRAP_MFN(CScriptSocket, Connect), asCALL_GENERIC); assert(r >= 0);
	r = engine->RegisterObjectMethod("socket", "int send(const string &in data)", WRAP_MFN(CScriptSocket, Send), asCALL_GENERIC); assert(r >= 0);
	r = engine->RegisterObjectMethod("socket", "string receive(int64 timeout = 0)", WRAP_MFN(CScriptSocket, Receive), asCALL_GENERIC); assert(r >= 0);
	r = engine->RegisterObjectMethod("socket", "bool isActive() const", WRAP_MFN(CScriptSocket, IsActive), asCALL_GENERIC); assert(r >= 0);

	return 0;
}

int RegisterScriptSocket(asIScriptEngine* engine)
{
	if (strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY"))
		return RegisterScriptSocket_Generic(engine);
	else
		return RegisterScriptSocket_Native(engine);
}

#else

int RegisterScriptSocket(asIScriptEngine* engine)
{
	return asNOT_SUPPORTED;
}

#endif

END_AS_NAMESPACE

