// The direct link (issue #8) on its own, with no emulator: an instance listens, a second one takes
// the next port, an editor connects, reads the hello line and exchanges bytes both ways, a second
// editor is turned away, and closing the link is seen from the other side.

#include "directlink.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#	include <winsock2.h>
#	include <ws2tcpip.h>
	using Sock = SOCKET;
	static void closeSock(Sock _s) { closesocket(_s); }
#else
#	include <arpa/inet.h>
#	include <netinet/in.h>
#	include <sys/socket.h>
#	include <unistd.h>
	using Sock = int;
	static void closeSock(Sock _s) { ::close(_s); }
#endif

static int failures = 0;
#define CHECK(c) do { if(!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++failures; } } while(0)

static Sock connectTo(int _port)
{
	const auto s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in addr {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(static_cast<uint16_t>(_port));
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if(::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
	{
		closeSock(s);
		return static_cast<Sock>(-1);
	}
	return s;
}

// Reads what the editor side has, waiting a little for it.
static std::string readSome(Sock _s, size_t _want)
{
	std::string got;
	char buf[256];
	for(int i = 0; i < 200 && got.size() < _want; ++i)
	{
		const auto n = ::recv(_s, buf, sizeof(buf), 0);
		if(n <= 0)
			break;
		got.append(buf, static_cast<size_t>(n));
	}
	return got;
}

int main()
{
	using namespace g1app;
	using namespace std::chrono_literals;

	DirectLink first, second;
	std::string error;
	CHECK(first.start("G1-Emu", error));
	CHECK(second.start("G1-Emu 2", error));
	CHECK(first.port() >= DirectLink::kBasePort);
	CHECK(second.port() > first.port());   // the second instance took the next free port

	const auto editor = connectTo(first.port());
	CHECK(static_cast<long long>(editor) >= 0);

	std::vector<uint8_t> in;
	for(int i = 0; i < 100 && !first.editorConnected(); ++i)
	{
		first.poll(in);
		std::this_thread::sleep_for(5ms);
	}
	CHECK(first.editorConnected());

	const std::string hello = "G1-Emu " + std::to_string(DirectLink::kProtocolVersion) + " G1-Emu\n";
	CHECK(readSome(editor, hello.size()) == hello);

	// Editor -> emulator: an IAm, as NME sends it.
	const uint8_t iam[] = { 0xf0, 0x33, 0x40, 0x06, 0x00, 0x03, 0x03, 0xf7 };
	CHECK(::send(editor, reinterpret_cast<const char*>(iam), sizeof(iam), 0) == static_cast<int>(sizeof(iam)));
	for(int i = 0; i < 100 && in.size() < sizeof(iam); ++i)
	{
		first.poll(in);
		std::this_thread::sleep_for(5ms);
	}
	CHECK(in == std::vector<uint8_t>(iam, iam + sizeof(iam)));

	// Emulator -> editor.
	const std::vector<uint8_t> reply = { 0xf0, 0x33, 0x40, 0x06, 0x01, 0x03, 0x03, 0xf7 };
	first.send(reply);
	CHECK(readSome(editor, reply.size()) == std::string(reply.begin(), reply.end()));

	// A second editor is turned away: its connection is closed without a hello.
	const auto intruder = connectTo(first.port());
	for(int i = 0; i < 20; ++i)
	{
		first.poll(in);
		std::this_thread::sleep_for(5ms);
	}
	CHECK(readSome(intruder, 1).empty());
	closeSock(intruder);

	// The editor going away frees the link for the next one.
	closeSock(editor);
	for(int i = 0; i < 100 && first.editorConnected(); ++i)
	{
		first.poll(in);
		std::this_thread::sleep_for(5ms);
	}
	CHECK(!first.editorConnected());

	first.stop();
	second.stop();
	CHECK(!first.listening());

	std::printf(failures ? "directlinktest: %d failure(s)\n" : "directlinktest: ok\n", failures);
	return failures ? 1 : 0;
}
