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

#pragma once

#include "inputleap/clipboard_types.h"

#include <cstdint>
#include <array>
#include <string>

#define CLIPBOARD_CHUNK_META_SIZE 7

namespace inputleap {

class IStream;

class ClipboardChunk {
public:

    static ClipboardChunk start(ClipboardID id, std::uint32_t sequence, const std::size_t& size);
    static ClipboardChunk data(ClipboardID id, std::uint32_t sequence, const std::string& data);
    static ClipboardChunk end(ClipboardID id, std::uint32_t sequence);

    std::uint8_t id_ = 0;
    std::uint32_t sequence_ = 0;
    std::uint8_t mark_ = 0;
    std::string data_;

};

class ClipboardAssembler {
public:
    int assemble(IStream* stream, ClipboardID& id, std::uint32_t& sequence, size_t maxSize);
    int accept(ClipboardID id, std::uint32_t sequence, std::uint8_t mark,
               const std::string& data, size_t maxSize);
    size_t expectedSize(ClipboardID id) const { return states_[id].expected; }
    const std::string& data(ClipboardID id) const { return states_[id].data; }

private:
    struct State {
        std::string data;
        size_t expected = 0;
        std::uint32_t sequence = 0;
        bool active = false;
    };
    std::array<State, kClipboardEnd> states_;
};

} // namespace inputleap
