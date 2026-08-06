#include "CNetGamePlayer.hpp"

#include <cstddef>
#include <cstdint>

#include "CPlayerInfo.hpp"
#include "netConnectionManager.hpp"
#include "pointers.hpp"

namespace
{
	// Fallback for 1.73. The live value is derived from CNetGamePlayer::GetGamerInfo at scan time.
	ptrdiff_t cnetgameplayer_player_info_offset = 0xE8;

	template <typename T>
	bool isPlausibleUserPointer(const T* ptr) noexcept
	{
		const auto address = reinterpret_cast<uintptr_t>(ptr);
		return address >= 0x10000 && address <= 0x00007FFFFFFFFFFF;
	}

	CPlayerInfo* getPlayerInfoPtr(CNetGamePlayer* player) noexcept
	{
		__try
		{
			auto* player_info = *reinterpret_cast<CPlayerInfo**>(reinterpret_cast<std::byte*>(player) + cnetgameplayer_player_info_offset);
			return isPlausibleUserPointer(player_info) ? player_info : nullptr;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return nullptr;
		}
	}

	const CPlayerInfo* getPlayerInfoPtr(const CNetGamePlayer* player) noexcept
	{
		__try
		{
			auto* player_info = *reinterpret_cast<CPlayerInfo* const*>(reinterpret_cast<const std::byte*>(player) + cnetgameplayer_player_info_offset);
			return isPlausibleUserPointer(player_info) ? player_info : nullptr;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return nullptr;
		}
	}
}

namespace rage
{
	const rlGamerInfo* netPlayer::GetGamerInfoImpl() const
	{
		return reinterpret_cast<const CNetGamePlayer*>(this)->GetGamerInfoImpl();
	}

	rage::netEndpoint* netPlayer::getEndpoint() const
	{
		return (*Stand::pointers::connection_mgr)->getEndpointById(endpoint_id);
	}
}

void CNetGamePlayer::setPlayerInfoOffset(const ptrdiff_t offset) noexcept
{
	if (offset > 0 && offset < 0x1000)
	{
		cnetgameplayer_player_info_offset = offset;
	}
}

rage::rlGamerInfo* CNetGamePlayer::GetGamerInfoImpl()
{
	if (auto* player_info = GetPlayerInfoImpl())
	{
		return &player_info->gamer_info;
	}
	return nullptr;
}

const rage::rlGamerInfo* CNetGamePlayer::GetGamerInfoImpl() const
{
	if (const auto* player_info = GetPlayerInfoImpl())
	{
		return &player_info->gamer_info;
	}
	return nullptr;
}

CPlayerInfo* CNetGamePlayer::GetPlayerInfoImpl()
{
	return getPlayerInfoPtr(this);
}

const CPlayerInfo* CNetGamePlayer::GetPlayerInfoImpl() const
{
	return getPlayerInfoPtr(this);
}

bool CNetGamePlayer::isHost() const
{
	if (const auto* gamer_info = GetGamerInfoImpl())
	{
		return gamer_info->isHost();
	}
	return false;
}
