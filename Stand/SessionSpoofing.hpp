#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "fwddecl.hpp"

#include "rlSessionInfo.hpp"

namespace Stand
{
	class SessionSpoofing
	{
	public:
		inline static int64_t copy_from_rid = 0;
		inline static rage::rlSessionInfo random_session_info{};
		inline static std::string og_peeraddr{};

		[[nodiscard]] static bool hasCopyFrom() noexcept;
		[[nodiscard]] static int64_t getCopyFromRid() noexcept;
		[[nodiscard]] static RemoteGamer* getCopyFrom(bool refresh = false) noexcept;
		static void setCopyFrom(RemoteGamer* g) noexcept;
		static void clearCopyFrom() noexcept;

		[[nodiscard]] static rage::rlSessionInfo& getRealSessionInfo();
		[[nodiscard]] static uint64_t getRealSessionId();
		[[nodiscard]] static int64_t getRealSessionType();

		[[nodiscard]] static bool isActive() noexcept;

		static void onDisable();
	private:
		static void cleanup();
	public:
		static void onTick();
	private:
		static void onTickActive();
	};
}
