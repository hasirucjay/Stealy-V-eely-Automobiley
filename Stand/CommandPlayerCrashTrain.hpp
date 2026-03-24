#pragma once

#include "CommandPlayerCrash.hpp"

#include "ComponentCrash.hpp"
#include "Hooking.hpp"
#include "Script.hpp"
#include "Util.hpp"

namespace Stand
{
	class CommandPlayerCrashTrain : public CommandPlayerCrash
	{
	public:
		explicit CommandPlayerCrashTrain(CommandList* const parent)
			: CommandPlayerCrash(parent, LOC("PLYCRSH_T"), CMDNAMES_OBF("steamroll"), HAVE_SEND_CLONE_SYNC_HOOK ? LOC("PLYCRSH_H_AOE2") : NOLABEL)
		{
		}

		void expandDong(std::vector<AbstractPlayer>&& targets) const final
		{
			if (targets.empty())
			{
				return;
			}
			auto p = targets.at(0);
			if (p.isRockstarAdmin()
				|| !setWhitelistTargeting(g_hooking.outgoing_train_create_exclusive, targets)
				)
			{
				return;
			}
			if (g_hooking.outgoing_player_sync_exclusive.isActive())
			{
				g_hooking.outgoing_train_create_exclusive.reset();
				return;
			}
			g_hooking.outgoing_player_sync_exclusive.add(p.getCNetGamePlayer());
			g_player_ent.setPos(p.getPos());

			STREAMING::REQUEST_MODEL(ATSTRINGHASH("freightcar"));
			while (!STREAMING::HAS_MODEL_LOADED(ATSTRINGHASH("freightcar")))
			{
				Script::current()->yield();
			}
			if (g_comp_crash.car1.isValid())
			{
				g_hooking.outgoing_train_create_exclusive.reset();
				g_hooking.outgoing_player_sync_exclusive.reset();
				return;
			}
			g_comp_crash.car1 = createCar(p);
			g_comp_crash.car2 = createCar(p);
			g_comp_crash.car3 = createCar(p);
			Script::current()->yield(1000);
			g_comp_crash.car1.giveControl(p);
			g_comp_crash.car2.giveControl(p);
			g_comp_crash.car3.giveControl(p);
			Script::current()->yield(1000);

			g_hooking.outgoing_player_sync_exclusive.reset();
			Script::current()->yield(500);

			g_comp_crash.car1.removeFromPlaneOfExistence();
			g_comp_crash.car1.reset();
			g_comp_crash.car2.removeFromPlaneOfExistence();
			g_comp_crash.car2.reset();
			g_comp_crash.car3.removeFromPlaneOfExistence();
			g_comp_crash.car3.reset();

			g_hooking.outgoing_train_create_exclusive.reset();
		}

	protected:
		[[nodiscard]] static AbstractEntity createCar(AbstractPlayer p)
		{
			auto car = Util::createVehicle(ATSTRINGHASH("freightcar"), p.getPos(), 0.0f);
			ENTITY::SET_ENTITY_VISIBLE(car, FALSE, 0);
			ENTITY::SET_ENTITY_COMPLETELY_DISABLE_COLLISION(car, FALSE, FALSE);
			return car;
		}
	};
}
