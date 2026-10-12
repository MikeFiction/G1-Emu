#include "hostlog.h"
#include "hostconfig.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>

namespace g1app
{
	namespace
	{
		std::mutex g_mutex;
		// Halved once past this, not trimmed to it exactly: cheap enough done rarely, too fine a
		// cap would mean rewriting the file almost every line near the limit.
		constexpr std::uintmax_t g_capBytes = 256 * 1024;
	}

	std::string logPath()
	{
		return (std::filesystem::path(defaultSettingsPath()).parent_path() / "g1-emu.log").string();
	}

	void logEvent(const std::string& _line)
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		const auto path = logPath();
		std::error_code ec;
		std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

		const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::tm tm{};
#ifdef _WIN32
		localtime_s(&tm, &now);
#else
		localtime_r(&now, &tm);
#endif
		char stamp[32];
		std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm);

		{
			std::ofstream out(path, std::ios::app);
			out << stamp << "  " << _line << "\n";
		}

		if(std::filesystem::file_size(path, ec) > g_capBytes && !ec)
		{
			std::vector<std::string> lines;
			{
				std::ifstream in(path);
				std::string line;
				while(std::getline(in, line))
					lines.push_back(std::move(line));
			}
			const auto keepFrom = lines.size() / 2;
			std::ofstream out(path, std::ios::trunc);
			out << "-- older lines dropped, the log stays under " << (g_capBytes / 1024) << " KB --\n";
			for(auto i = keepFrom; i < lines.size(); ++i)
				out << lines[i] << "\n";
		}
	}
}
