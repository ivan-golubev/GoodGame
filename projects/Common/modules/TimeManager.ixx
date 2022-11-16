module;
#include <cstdint>
#include <chrono>
export module TimeManager;

namespace gg 
{
	using namespace std::chrono;
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
