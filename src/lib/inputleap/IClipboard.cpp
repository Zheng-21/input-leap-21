/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2004 Chris Schoeneman
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

#include "inputleap/IClipboard.h"
#include <cassert>
#include <vector>

namespace inputleap {

bool IClipboard::unmarshall(IClipboard* clipboard, const std::string& data, Time time)
{
    assert(clipboard != nullptr);

    // Validate the complete payload before changing the destination clipboard.
    if (data.size() < 4) return false;
    const char* index = data.data();
    size_t remaining = data.size();
    const std::uint32_t numFormats = readUInt32(index);
    index += 4;
    remaining -= 4;
    for (std::uint32_t i = 0; i < numFormats; ++i) {
        if (remaining < 8) return false;
        const std::uint32_t size = readUInt32(index + 4);
        index += 8;
        remaining -= 8;
        if (size > remaining) return false;
        index += size;
        remaining -= size;
    }
    if (remaining != 0) return false;
    index = data.data();

    if (clipboard->open(time)) {
        // clear existing data
        if (!clipboard->clear()) {
            clipboard->close();
            return false;
        }

        // read the number of formats
        const std::uint32_t numFormats = readUInt32(index);
        index += 4;

        // read each format
        for (std::uint32_t i = 0; i < numFormats; ++i) {
            // get the format id
            const std::uint32_t format = readUInt32(index);
            index += 4;

            // get the size of the format data
            std::uint32_t size = readUInt32(index);
            index += 4;

            // save the data if it's a known format.  if either the client
            // or server supports more clipboard formats than the other
            // then one of them will get a format >= kNumFormats here.
            if (format < IClipboard::kNumFormats) {
                clipboard->add(static_cast<IClipboard::EFormat>(format), std::string(index, size));
            }
            index += size;
        }

        // done
        const bool success = clipboard->succeeded();
        clipboard->close();
        return success;
    }
    return false;
}

std::string IClipboard::marshall(const IClipboard* clipboard)
{
    // return data format:
    // 4 bytes => number of formats included
    // 4 bytes => format enum
    // 4 bytes => clipboard data size n
    // n bytes => clipboard data
    // back to the second 4 bytes if there is another format

    assert(clipboard != nullptr);

    std::string data;

    std::vector<std::string> formatData;
    formatData.resize(IClipboard::kNumFormats);
    // FIXME -- use current time
    if (clipboard->open(0)) {

        // compute size of marshalled data
        std::uint32_t size = 4;
        std::uint32_t numFormats = 0;
        for (std::uint32_t format = 0; format != IClipboard::kNumFormats; ++format) {
            if (clipboard->has(static_cast<IClipboard::EFormat>(format))) {
                ++numFormats;
                formatData[format] =
                    clipboard->get(static_cast<IClipboard::EFormat>(format));
                size += 4 + 4 + static_cast<std::uint32_t>(formatData[format].size());
            }
        }

        // allocate space
        data.reserve(size);

        // marshall the data
        writeUInt32(&data, numFormats);
        for (std::uint32_t format = 0; format != IClipboard::kNumFormats; ++format) {
            if (clipboard->has(static_cast<IClipboard::EFormat>(format))) {
                writeUInt32(&data, format);
                writeUInt32(&data, static_cast<std::uint32_t>(formatData[format].size()));
                data += formatData[format];
            }
        }
        clipboard->close();
    }

    return data;
}

bool
IClipboard::copy(IClipboard* dst, const IClipboard* src)
{
    assert(dst != nullptr);
    assert(src != nullptr);

    return copy(dst, src, src->getTime());
}

bool
IClipboard::copy(IClipboard* dst, const IClipboard* src, Time time)
{
    assert(dst != nullptr);
    assert(src != nullptr);

    bool success = false;
    if (src->open(time)) {
        std::vector<std::pair<EFormat, std::string>> formats;
        for (std::int32_t format = 0; format != IClipboard::kNumFormats; ++format) {
            EFormat eFormat = static_cast<EFormat>(format);
            if (src->has(eFormat)) formats.emplace_back(eFormat, src->get(eFormat));
        }
        if (src->succeeded() && dst->open(time)) {
            if (dst->clear()) {
                for (const auto& format : formats) dst->add(format.first, format.second);
                success = dst->succeeded();
            }
            dst->close();
        }
        src->close();
    }

    return success;
}

std::uint32_t IClipboard::readUInt32(const char* buf)
{
    const unsigned char* ubuf = reinterpret_cast<const unsigned char*>(buf);
    return (static_cast<std::uint32_t>(ubuf[0]) << 24) |
           (static_cast<std::uint32_t>(ubuf[1]) << 16) |
           (static_cast<std::uint32_t>(ubuf[2]) <<  8) |
            static_cast<std::uint32_t>(ubuf[3]);
}

void IClipboard::writeUInt32(std::string* buf, std::uint32_t v)
{
    *buf += static_cast<std::uint8_t>((v >> 24) & 0xff);
    *buf += static_cast<std::uint8_t>((v >> 16) & 0xff);
    *buf += static_cast<std::uint8_t>((v >>  8) & 0xff);
    *buf += static_cast<std::uint8_t>( v & 0xff);
}

} // namespace inputleap
