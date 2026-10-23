// coop/dev/drill_chat.cpp -- see coop/dev/drill_chat.h.

#include "coop/dev/drill_chat.h"

#include "coop/comms/chat_feed.h"
#include "coop/comms/chat_sync.h"

#include <cstdint>
#include <string_view>

namespace coop::dev::drill_chat {

void Say(const std::string& line) { coop::chat_sync::QueueSend(line); }

bool ChatHasLive(const std::string& needle) {
    bool found = false;
    coop::chat_feed::ForEachRow([&](const coop::chat_feed::RowView& r) {
        if (!r.retained && r.line.find(needle) != std::string_view::npos) found = true;
    });
    return found;
}

bool PollDue() {
    static uint32_t sTicks = 0;
    return sTicks++ % 15 == 0;
}

}  // namespace coop::dev::drill_chat
