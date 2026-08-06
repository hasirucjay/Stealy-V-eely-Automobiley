#pragma once

#include "CommandToggle.hpp"

#include "AbstractEntity.hpp"
#include "CommandListSelectVehicleModel.hpp"
#include "CommandSlider.hpp"
#include "FiberPool.hpp"
#include "input.hpp"
#include "natives.hpp"
#include "Script.hpp"
#include "Util.hpp"
#include "using_model.hpp"

namespace Stand
{
	class CommandVehicleGunToggle : public CommandToggle
	{
	private:
		int cooldown = 0;

		void doThing(int ticks)
		{
			const auto hash = model->getVehicle();
			using_model(hash, [this, hash]
			{
				auto cam_rot = CAMERA::GET_FINAL_RENDERED_CAM_ROT(2);
				auto cam_pos = CAMERA::GET_FINAL_RENDERED_CAM_COORD();
				auto dir = cam_rot.toDir();
				auto spawn_pos = v3(cam_pos.x + dir.x * 5.0f, cam_pos.y + dir.y * 5.0f, cam_pos.z + dir.z * 5.0f);
				auto heading = ENTITY::GET_ENTITY_HEADING(g_player_ped);
				auto veh = Util::createVehicle(hash, spawn_pos, heading);
				if (veh.isValid())
				{
					ENTITY::SET_ENTITY_ROTATION(veh, cam_rot.x, cam_rot.y, cam_rot.z, 2, TRUE);
					VEHICLE::SET_VEHICLE_GRAVITY(veh, false);
					VEHICLE::SET_VEHICLE_FORWARD_SPEED(veh, (float)speed->value);
					const int dist = distance->value;
					FiberPool::queueJob([veh, dist]() mutable
					{
						Script::current()->yield(dist * 1000);
						veh.removeFromPlaneOfExistence();
					});
				}
			});
			cooldown = ticks;
		}

	public:
		CommandListSelectVehicleModel* model;
		CommandSlider* speed;
		CommandSlider* distance;

		explicit CommandVehicleGunToggle(CommandList* const parent)
			: CommandToggle(parent, LOC("VEHGUN"), CMDNAMES_OBF("vehiclegun"), LOC("VEHGUN_H"))
		{
		}

		void onEnable(Click& click) final
		{
			registerScriptTickEventHandler(click, [this]
			{
				if (!m_on)
				{
					return false;
				}
				if (cooldown > 0)
				{
					--cooldown;
				}
				else if (Input::isCommandInputAllowed() && Input::isPressingAttack())
				{
					doThing(10);
				}
				return true;
			});
		}
	};
}
