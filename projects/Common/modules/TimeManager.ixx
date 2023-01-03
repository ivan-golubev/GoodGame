module;
#include <cstdint>
#include <chrono>
export module TimeManager;

using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;
using std::chrono::time_point;

namespace gg
{
	export class TimeManager
	{
	public:
		[[nodiscard("Delta time should be passed to renderer")]]
		milliseconds Tick();
		milliseconds GetCurrentTimeMs() const;
		seconds GetCurrentTimeSec() const;
	private:
		time_point<steady_clock> startTime{ steady_clock::now() };
		time_point<steady_clock> currentTime{ startTime };
	};
} // namespace gg
