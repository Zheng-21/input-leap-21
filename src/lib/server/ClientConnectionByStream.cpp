/*  InputLeap -- mouse and keyboard sharing utility
    Copyright (C) InputLeap contributors

    This package is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    found in the file LICENSE that should have accompanied this file.

    This package is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ClientConnectionByStream.h"
#include "base/Log.h"
#include "inputleap/ClipboardChunk.h"
#include "inputleap/FileChunk.h"
#include "inputleap/ProtocolUtil.h"
#include "inputleap/protocol_types.h"
#include "io/IStream.h"
#include <limits>

namespace inputleap {

ClientConnectionByStream::ClientConnectionByStream(std::unique_ptr<IStream> stream,
                                                   IEventQueue* events) :
    stream_{std::move(stream)}, events_{events}
{
    events_->add_handler(EventType::CLIENT_MOUSE_FLUSH, &mouse_target_, [this](const auto&) {
        mouse_flush_pending_ = false;
        flush_mouse();
    });
}

ClientConnectionByStream::~ClientConnectionByStream()
{
    events_->remove_handler(EventType::CLIENT_MOUSE_FLUSH, &mouse_target_);
}

void ClientConnectionByStream::queue_mouse(bool relative, std::int32_t x, std::int32_t y)
{
    if (mouse_pending_ && mouse_relative_ != relative) {
        flush_mouse();
    }
    if (mouse_pending_ && relative) {
        const auto dx = static_cast<std::int64_t>(mouse_x_) + x;
        const auto dy = static_cast<std::int64_t>(mouse_y_) + y;
        // Relative protocol coordinates are signed 16-bit. Flush before summing
        // would overflow the wire representation, retaining every displacement.
        if (dx < (std::numeric_limits<std::int16_t>::min)() ||
            dx > (std::numeric_limits<std::int16_t>::max)() ||
            dy < (std::numeric_limits<std::int16_t>::min)() ||
            dy > (std::numeric_limits<std::int16_t>::max)()) {
            flush_mouse();
        }
    }
    if (!mouse_pending_) {
        mouse_queued_at_ = mouse_wait_stats_.enabled() ? InputStatistics::Clock::now() :
                                                      InputStatistics::Clock::time_point{};
        mouse_x_ = mouse_y_ = 0;
        mouse_count_ = 0;
    }
    mouse_x_ = relative ? mouse_x_ + x : x;
    mouse_y_ = relative ? mouse_y_ + y : y;
    mouse_relative_ = relative;
    mouse_pending_ = true;
    ++mouse_count_;
    if (!mouse_flush_pending_) {
        mouse_flush_pending_ = true;
        events_->add_event(EventType::CLIENT_MOUSE_FLUSH, &mouse_target_);
    }
}

void ClientConnectionByStream::flush_mouse()
{
    if (!mouse_pending_) {
        return;
    }
    ProtocolUtil::writef(stream_.get(), mouse_relative_ ? kMsgDMouseRelMove : kMsgDMouseMove,
                         mouse_x_, mouse_y_);
    mouse_pending_ = false;
    if (mouse_wait_stats_.enabled()) {
        mouse_wait_stats_.record("send-mouse-wait", "ms",
            std::chrono::duration<double, std::milli>(InputStatistics::Clock::now() - mouse_queued_at_).count());
        mouse_merge_stats_.record("send-mouse-merged", "events", mouse_count_);
    }
}

const EventTarget* ClientConnectionByStream::get_event_target()
{
    return stream_->get_event_target();
}

void ClientConnectionByStream::send_query_info_1_6()
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgQInfo);
}

void ClientConnectionByStream::send_enter_1_6(std::int32_t x_abs, std::int32_t y_abs,
                                              std::uint32_t seq_num, KeyModifierMask mask)
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgCEnter, x_abs, y_abs, seq_num, mask);
}

void ClientConnectionByStream::send_leave_1_6()
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgCLeave);
}

void ClientConnectionByStream::send_key_down_1_6(KeyID key, KeyModifierMask mask, KeyButton button)
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgDKeyDown, key, mask, button);
}

void ClientConnectionByStream::send_key_up_1_6(KeyID key, KeyModifierMask mask, KeyButton button)
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgDKeyUp, key, mask, button);
}

void ClientConnectionByStream::send_key_repeat_1_6(KeyID key, KeyModifierMask mask,
                                                   std::int32_t count, KeyButton button)
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgDKeyRepeat, key, mask, count, button);
}

void ClientConnectionByStream::send_mouse_down_1_6(ButtonID button)
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgDMouseDown, button);
}

void ClientConnectionByStream::send_mouse_up_1_6(ButtonID button)
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgDMouseUp, button);
}

void ClientConnectionByStream::send_mouse_move_1_6(std::int32_t x_abs, std::int32_t y_abs)
{
    queue_mouse(false, x_abs, y_abs);
}

void ClientConnectionByStream::send_mouse_relative_move_1_6(std::int32_t x_rel, std::int32_t y_rel)
{
    queue_mouse(true, x_rel, y_rel);
}

void ClientConnectionByStream::send_mouse_wheel_1_6(std::int32_t x_delta, std::int32_t y_delta)
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgDMouseWheel, x_delta, y_delta);
}

void ClientConnectionByStream::send_drag_info_1_6(std::uint32_t file_count, const std::string& data)
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgDDragInfo, file_count, &data);
}

void ClientConnectionByStream::send_screensaver_1_6(bool on)
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgCScreenSaver, on ? 1 : 0);
}

void ClientConnectionByStream::send_reset_options_1_6()
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgCResetOptions);
}

void ClientConnectionByStream::send_set_options_1_6(const OptionsList& options)
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgDSetOptions, &options);
}

void ClientConnectionByStream::send_info_ack_1_6()
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgCInfoAck);
}

void ClientConnectionByStream::send_keep_alive_1_6()
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgCKeepAlive);
}

void ClientConnectionByStream::send_close_1_6(const char* msg)
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), msg);
}

void ClientConnectionByStream::send_clipboard_chunk_1_6(const ClipboardChunk& chunk)
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgDClipboard, chunk.id_, chunk.sequence_, chunk.mark_,
                         &chunk.data_);
}

void ClientConnectionByStream::send_file_chunk_1_6(const FileChunk& chunk)
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgDFileTransfer, chunk.mark_, &chunk.data_);
}

void ClientConnectionByStream::send_grab_clipboard(ClipboardID id)
{
    flush_mouse();
    ProtocolUtil::writef(stream_.get(), kMsgCClipboard, id, 0);
}

void ClientConnectionByStream::flush()
{
    flush_mouse();
    stream_->flush();
}

void ClientConnectionByStream::close()
{
    mouse_pending_ = false;
    events_->remove_handler(EventType::CLIENT_MOUSE_FLUSH, &mouse_target_);
    stream_->close();
}

} // namespace inputleap
