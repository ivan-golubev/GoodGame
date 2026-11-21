module;
#include <cstdint>
export module ObjectId;

export namespace gg
{
	/* Identifies a runtime game object (model, skybox, camera) for its whole lifetime. The Scene hands ids out
	   from 1 upwards, from one sequence for all kinds of objects, so no two objects share an id.
	   noObjectId (0) means "no object". */
	using ObjectId = uint32_t;

	constexpr ObjectId noObjectId{ 0 };
} // namespace gg
