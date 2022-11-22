module;
#include <cstdint>
#include <chrono>
export module TimeManager;

using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::system_clock;
using std::chrono::time_point;

namespace gg 
{
	export class TimeManager {
	public:
		TimeManager();
		milliseconds Tick();
		milliseconds GetCurrentTimeMs() const;
		seconds GetCurrentTimeSec() const;
	private:
		time_point<system_clock> currentTime{};
		time_point<system_clock> startTime{};
	};
} // namespace gg
