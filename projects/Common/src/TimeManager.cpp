module;
#include <cstdint>
#include <cstdlib>
#include <chrono>
#include <windows.h>
module TimeManager;

namespace gg
{
	using namespace std::chrono;

	TimeManager::TimeManager()
		: startTime{ system_clock::now() }
		, currentTime{ startTime }
	{
	}

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