#include "ColoadMgr.hpp"

#include <string>

#ifdef STAND_DEV
#include "FileLogger.hpp"
#endif
#include "pointers.hpp"

namespace Stand
{
	namespace
	{
		[[nodiscard]] bool containsHook(const std::string_view hooks, const Codename& name)
		{
			const std::string token = name.toString();
			size_t pos = hooks.find(token);
			while (pos != std::string_view::npos)
			{
				const bool starts_token = (pos == 0 || hooks[pos - 1] == ' ' || hooks[pos - 1] == ',');
				const size_t end = pos + token.size();
				const bool ends_token = (end == hooks.size() || hooks[end] == ' ' || hooks[end] == ',');
				if (starts_token && ends_token)
				{
					return true;
				}
				pos = hooks.find(token, pos + 1);
			}
			return false;
		}
	}

	void ColoadMgr::addCheck(const Codename& cn, void* addr, const void* data, size_t size)
	{
		std::vector<uint8_t> vec;
		vec.assign((const uint8_t*)data, (const uint8_t*)data + size);
		existing_hook_checks.emplace_front(ExistingHookCheck{ cn, addr, std::move(vec) });
	}

	void ColoadMgr::classifyExistingHooks(const std::string_view hooks)
	{
		if (hooks.empty())
		{
			return;
		}

		coloading_with_any_menu = true;
		existing_hook_names.assign(hooks);

		// Cherax consistently owns both network ingress hooks and several crash-patch hooks.
		// Requiring the whole core plus multiple patch anchors avoids identifying a menu from
		// one commonly shared detour while still working when HC has already been patched out.
		const bool has_core = containsHook(hooks, Codename("HJ"))
			&& containsHook(hooks, Codename("PJ"))
			&& containsHook(hooks, Codename("Q0"));
		const unsigned int patch_anchors = containsHook(hooks, Codename("X3"))
			+ containsHook(hooks, Codename("X7"))
			+ containsHook(hooks, Codename("X9"))
			+ containsHook(hooks, Codename("XM"))
			+ containsHook(hooks, Codename("VA"));
		if (has_core && patch_anchors >= 2)
		{
			coloading_with_cherax = true;
#ifdef STAND_DEV
			g_logger.log(soup::ObfusString("Co-load identified as Cherax from hook fingerprint."));
#endif
		}
	}

	bool ColoadMgr::shouldYieldHook(const Codename& name)
	{
		if (!coloading_with_cherax || !containsHook(existing_hook_names, name))
		{
			return false;
		}

		const auto matches = [&name](const Codename& expected)
		{
			return name.category == expected.category && name.id == expected.id;
		};
		return matches(Codename("P7")) // model lookup
			|| matches(Codename("PJ"))  // incoming clone sync
			|| matches(Codename("PK"))  // clone packing
			|| matches(Codename("RK"))  // player-data serialisation
			|| matches(Codename("Q0")); // raw packet dispatch
	}

	void ColoadMgr::check()
	{
		if (coloading_with_cherax)
		{
			return;
		}

		for (auto i = existing_hook_checks.head.load(); i != nullptr; i = i->next)
		{
			if (i->data.addr != nullptr && memcmp(i->data.addr, i->data.vec.data(), i->data.vec.size()) != 0)
			{
				coloading_with_any_menu = true;
#ifdef STAND_DEV
				g_logger.log(fmt::format(fmt::runtime(soup::ObfusString("Co-load detected ({})").str()), i->data.name.toString()));
#endif
				break;
			}
		}

		if (pointers::request_control_spectating_check)
		{
			if (pointers::request_control_spectating_check[-2] != 0x8B
				|| pointers::request_control_spectating_check[-1] != 0xD9
				|| pointers::request_control_spectating_check[0] != 0xE8
				|| pointers::request_control_spectating_check[7 - 2] != 0x84
				|| pointers::request_control_spectating_check[8 - 2] != 0xC0
				|| pointers::request_control_spectating_check[9 - 2] != 0x75
				|| pointers::request_control_spectating_check[10 - 2] != 0x6A
				|| pointers::request_control_spectating_check[11 - 2] != 0x8B
				|| pointers::request_control_spectating_check[12 - 2] != 0xCB
				|| pointers::request_control_spectating_check[13 - 2] != 0xE8
				)
			{
				coloading_with_any_menu = true;
				if (pointers::request_control_spectating_check[-2] == 0x8B
					&& pointers::request_control_spectating_check[-1] == 0xD9
					&& pointers::request_control_spectating_check[0] == 0xE8
					&& pointers::request_control_spectating_check[7 - 2] == 0x90
					&& pointers::request_control_spectating_check[8 - 2] == 0x90
					&& pointers::request_control_spectating_check[9 - 2] == 0x90
					&& pointers::request_control_spectating_check[10 - 2] == 0x90
					&& pointers::request_control_spectating_check[11 - 2] == 0x8B
					&& pointers::request_control_spectating_check[12 - 2] == 0xCB
					&& pointers::request_control_spectating_check[13 - 2] == 0xE8
					)
				{
					coloading_with_cherax = true;
#ifdef STAND_DEV
					g_logger.log(fmt::format(fmt::runtime(soup::ObfusString("Co-load detected ({})").str()), Codename("HC").toString()));
#endif
				}
				else
				{
#ifdef STAND_DEV
					g_logger.log(fmt::format(fmt::runtime(soup::ObfusString("Co-load detected ({})").str()), Codename("HD").toString()));
#endif
				}
			}
		}
	}
}
