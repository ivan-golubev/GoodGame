module;
#include <cstdint>
#include <cstdlib>
#include <chrono>
#include <windows.h>
module TimeManager;

using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::system_clock;
using std::chrono::time_point;

namespace gg
{
	milliseconds TimeManager::Tick()
	{
		time_point<system_clock> now = system_clock::now();
		milliseconds elapsed = duration_cast<milliseconds>(currentTime - now);
		currentTime = now;
		return elapsed;
	}

	milliseconds TimeManager::GetCurrentTimeMs() const
	{
		return duration_cast<milliseconds>(currentTime - startTime);
	}

	seconds TimeManager::GetCurrentTimeSec() const
	{
		return duration_cast<seconds>(currentTime - startTime);
	}
} // namespace gg