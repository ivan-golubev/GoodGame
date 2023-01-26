module;
#include <cstdint>
#include <cstdlib>
#include <chrono>
#include <windows.h>
module TimeManager;

using std::chrono::nanoseconds;
using std::chrono::milliseconds;
using std::chrono::steady_clock;
using std::chrono::time_point;

using namespace std::chrono_literals;

namespace gg
{
	nanoseconds TimeManager::Tick()
	{
		time_point<steady_clock> now = steady_clock::now();
		nanoseconds elapsed = duration_cast<nanoseconds>(now - currentTime);
		currentTime = now;
		return elapsed;
	}

	nanoseconds TimeManager::GetCurrentTimeUs() const
	{
		return duration_cast<nanoseconds>(currentTime - startTime);
	}

	milliseconds TimeManager::GetCurrentTimeMs() const
	{
		return duration_cast<milliseconds>(currentTime - startTime);
	}

	double TimeManager::GetCurrentTimeSec() const
	{
		return GetCurrentTimeUs() / 1.0s;
	}
} // namespace gg