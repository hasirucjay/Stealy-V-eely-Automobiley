#include "unrestrictGameplayCam.hpp"

#include <soup/Bytepatch.hpp>

#include "natives.hpp"
#include "pointers.hpp"

namespace Stand
{
	void unrestrictGameplayCam()
	{
		{
			soup::Bytepatch patch_inst{};
			patch_inst.initPatchNOP(pointers::clamp_gameplay_cam_pitch, 55);
			CAMERA::SET_THIRD_PERSON_CAM_RELATIVE_PITCH_LIMITS_THIS_UPDATE(-180.0f, 180.0f);
		}
		CAMERA::SET_THIRD_PERSON_CAM_RELATIVE_HEADING_LIMITS_THIS_UPDATE(-180.0f, 180.0f);
	}
}
