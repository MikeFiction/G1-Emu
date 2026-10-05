#include "directlink.h"

#include <cstring>

#ifdef _WIN32
#	ifndef NOMINMAX
#		define NOMINMAX
#	endif
#	include <winsock2.h>
#	include <ws2tcpip.h>
#	pragma comment(lib, "ws2_32.lib")
	using socklen_t = int;
	namespace
	{
		using Socket = SOCKET;
		bool invalid(long long _s) { return _s < 0 || static_cast<Socket>(_s) == INVALID_SOCKET; }
		void closeSocket(long long _s) { closesocket(static_cast<Socket>(_s)); }
		bool setNonBlocking(Socket _s) { u_long on = 1; return ioctlsocket(_s, FIONBIO, &on) == 0; }
		bool wouldBlock() { const int e = WSAGetLastError(); return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS; }
		bool initSockets()
		{
			static const bool ok = []
			{
				WSADATA data;
				return WSAStartup(MAKEWORD(2, 2), &data) == 0;
			}();
			return ok;
		}
		constexpr int kSendFlags = 0;
	}
#else
#	include <arpa/inet.h>
#	include <cerrno>
#	include <fcntl.h>
#	include <netinet/in.h>
#	include <netinet/tcp.h>
#	include <sys/socket.h>
#	include <unistd.h>
	namespace
	{
		using Socket = int;
		bool invalid(long long _s) { return _s < 0; }
		void closeSocket(long long _s) { ::close(static_cast<Socket>(_s)); }
		bool setNonBlocking(Socket _s) { const int f = fcntl(_s, F_GETFL, 0); return f >= 0 && fcntl(_s, F_SETFL, f | O_NONBLOCK) == 0; }
		bool wouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR; }
		bool initSockets() { return true; }
#	ifdef MSG_NOSIGNAL
		constexpr int kSendFlags = MSG_NOSIGNAL;   // an editor that went away is not a SIGPIPE
#	else
		constexpr int kSendFlags = 0;
#	endif
	}
#endif

namespace g1app
{
	DirectLink::~DirectLink()
	{
		stop();
	}

	bool DirectLink::start(const std::string& _name, std::string& _error)
	{
		stop();
		if(!initSockets())
		{
			_error = "sockets are not available";
			return false;
		}

		for(int i = 0; i < kMaxInstances; ++i)
		{
			const auto s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if(invalid(static_cast<long long>(s)))
			{
				_error = "could not create a socket";
				return false;
			}

			sockaddr_in addr {};
			addr.sin_family = AF_INET;
			addr.sin_port = htons(static_cast<uint16_t>(kBasePort + i));
			addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // this machine only, never the network

			// A port another instance is listening on has to refuse us, so that the next instance
			// takes the next port; a port left in TIME_WAIT by an emulator that just quit must not.
			// On POSIX SO_REUSEADDR does exactly that. On Windows it would let us steal a listening
			// port, so there it is SO_EXCLUSIVEADDRUSE, and TIME_WAIT does not block a bind anyway.
			const int one = 1;
#ifdef _WIN32
			::setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&one), sizeof(one));
#else
			::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#endif
			if(::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0 && ::listen(s, 1) == 0
				&& setNonBlocking(s))
			{
				m_listen = static_cast<long long>(s);
				m_port = kBasePort + i;
				m_name = _name;
				return true;
			}
			closeSocket(static_cast<long long>(s));
		}
		_error = "ports " + std::to_string(kBasePort) + "-" + std::to_string(kBasePort + kMaxInstances - 1)
			+ " are all taken";
		return false;
	}

	void DirectLink::stop()
	{
		closeClient();
		if(!invalid(m_listen))
			closeSocket(m_listen);
		m_listen = -1;
		m_port = 0;
	}

	void DirectLink::closeClient()
	{
		if(!invalid(m_client))
			closeSocket(m_client);
		m_client = -1;
	}

	void DirectLink::poll(std::vector<uint8_t>& _in)
	{
		if(invalid(m_listen))
			return;

		// Someone knocking: the first editor stays, anyone after it is turned away.
		for(;;)
		{
			const auto c = ::accept(static_cast<Socket>(m_listen), nullptr, nullptr);
			if(invalid(static_cast<long long>(c)))
				break;
			if(!invalid(m_client))
			{
				closeSocket(static_cast<long long>(c));
				continue;
			}
			setNonBlocking(c);
			const int one = 1;
			::setsockopt(c, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof(one));
			m_client = static_cast<long long>(c);
			const auto hello = "G1-Emu " + std::to_string(kProtocolVersion) + " " + m_name + "\n";
			send(std::vector<uint8_t>(hello.begin(), hello.end()));
		}

		if(invalid(m_client))
			return;

		uint8_t buf[4096];
		for(;;)
		{
			const auto n = ::recv(static_cast<Socket>(m_client), reinterpret_cast<char*>(buf), sizeof(buf), 0);
			if(n > 0)
			{
				_in.insert(_in.end(), buf, buf + n);
				continue;
			}
			if(n < 0 && wouldBlock())
				return;
			closeClient();   // the editor closed the link, or it broke
			return;
		}
	}

	void DirectLink::send(const std::vector<uint8_t>& _bytes)
	{
		if(invalid(m_client) || _bytes.empty())
			return;

		size_t done = 0;
		int spins = 0;
		while(done < _bytes.size())
		{
			const auto n = ::send(static_cast<Socket>(m_client), reinterpret_cast<const char*>(_bytes.data() + done),
				static_cast<int>(_bytes.size() - done), kSendFlags);
			if(n > 0)
			{
				done += static_cast<size_t>(n);
				continue;
			}
			// A full socket buffer means an editor that has stopped reading. The G1's replies are
			// small and the buffer is large, so a short wait is enough; past that, let it go.
			if(n < 0 && wouldBlock() && ++spins < 1000)
				continue;
			closeClient();
			return;
		}
	}

	std::string DirectLink::describe() const
	{
		if(invalid(m_listen))
			return "direct link off";
		return "direct link on port " + std::to_string(m_port) + (editorConnected() ? " (editor connected)" : "");
	}
}
