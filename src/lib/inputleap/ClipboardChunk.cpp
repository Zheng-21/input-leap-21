/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2015-2016 Symless Ltd.
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "inputleap/ClipboardChunk.h"

#include "inputleap/ProtocolUtil.h"
#include "inputleap/protocol_types.h"
#include "io/IStream.h"
#include <limits>

namespace inputleap {

ClipboardChunk ClipboardChunk::start(ClipboardID id, std::uint32_t sequence,
                                     const std::size_t& size)
{
    ClipboardChunk chunk;
    chunk.id_ = id;
    chunk.sequence_ = sequence;
    chunk.mark_ = kDataStart;
    chunk.data_ = std::to_string(size);
    return chunk;
}

ClipboardChunk ClipboardChunk::data(ClipboardID id, std::uint32_t sequence,
                                    const std::string& data)
{
    ClipboardChunk chunk;
    chunk.id_ = id;
    chunk.sequence_ = sequence;
    chunk.mark_ = kDataChunk;
    chunk.data_ = data;
    return chunk;
}

ClipboardChunk ClipboardChunk::end(ClipboardID id, std::uint32_t sequence)
{
    ClipboardChunk chunk;
    chunk.id_ = id;
    chunk.sequence_ = sequence;
    chunk.mark_ = kDataEnd;
    return chunk;
}

int ClipboardAssembler::assemble(inputleap::IStream* stream, ClipboardID& id,
                                 std::uint32_t& sequence, size_t maxSize)
{
    std::uint8_t mark;
    std::string data;

    if (!ProtocolUtil::readf(stream, kMsgDClipboard + 4, &id, &sequence, &mark, &data)) {
        return kError;
    }
    return accept(id, sequence, mark, data, maxSize);
}

int ClipboardAssembler::accept(ClipboardID id, std::uint32_t sequence, std::uint8_t mark,
                               const std::string& data, size_t maxSize)
{
    if (id >= kClipboardEnd) return kError;
    State& state = states_[id];
    if (mark == kDataStart) {
        size_t expected = 0;
        if (data.empty()) {
            state.active = false;
            return kError;
        }
        for (char c : data) {
            if (c < '0' || c > '9' || expected > ((std::numeric_limits<size_t>::max)() - (c - '0')) / 10) {
                state.active = false;
                return kError;
            }
            expected = expected * 10 + (c - '0');
        }
        if (expected > maxSize) {
            state.active = false;
            return kError;
        }
        state.expected = expected;
        state.sequence = sequence;
        state.active = true;
        state.data.clear();
        return kStart;
    }
    if (!state.active || state.sequence != sequence) return kError;
    if (mark == kDataChunk) {
        if (data.size() > state.expected - state.data.size()) {
            state.active = false;
            return kError;
        }
        state.data.append(data);
        return kNotFinish;
    }
    else if (mark == kDataEnd) {
        state.active = false;
        if (state.expected != state.data.size()) {
            return kError;
        }
        return kFinish;
    }

    state.active = false;
    return kError;
}

} // namespace inputleap
