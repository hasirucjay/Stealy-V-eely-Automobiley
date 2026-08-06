#include "CommandHistoryCopy.hpp"

#include "CommandHistoricPlayer.hpp"
#include "PlayerHistory.hpp"
#include "RemoteGamer.hpp"
#include "SessionSpoofing.hpp"

namespace Stand
{
	CommandHistoryCopy::CommandHistoryCopy(CommandList* parent)
		: CommandReadonlyValue(parent, LOC("CPYSESS_F"))
	{
	}

	void CommandHistoryCopy::onClick(Click& click)
	{
		if (SessionSpoofing::hasCopyFrom())
		{
			if (auto* hp = PlayerHistory::findRID(SessionSpoofing::getCopyFromRid()))
			{
				PlayerHistory::findCommand(hp)->redirectOpen(click, this);
			}
		}
	}

	void CommandHistoryCopy::onTickInGameViewport()
	{
		if (SessionSpoofing::hasCopyFrom())
		{
			if (auto* hp = PlayerHistory::findRID(SessionSpoofing::getCopyFromRid()))
			{
				setValue(hp->name);
				return;
			}
			SessionSpoofing::clearCopyFrom();
		}
		setValue(LANG_GET_W("NA"));
	}

	void CommandHistoryCopy::onTickInWebViewport()
	{
		return onTickInWebViewportImplRedirect();
	}
}
