/* InputLeap -- clipboard failure and malformed-data checks.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "inputleap/Clipboard.h"
#include "inputleap/ClipboardChunk.h"
#include "inputleap/protocol_types.h"
#include <cassert>
#include <string>

using namespace inputleap;

class FailingClipboard : public Clipboard {
public:
    bool succeeded() const override { return false; }
};

int main()
{
    Clipboard destination;
    destination.open(0);
    destination.add(IClipboard::kText, "preserved");
    destination.close();
    const std::string truncated("\0\0\0\1\0\0\0\0\0\0\0\5abc", 15);
    assert(!destination.unmarshall(truncated, 1));

    FailingClipboard source;
    source.open(0);
    source.add(IClipboard::kText, "unreadable");
    source.close();
    assert(!Clipboard::copy(&destination, &source));
    destination.open(0);
    assert(destination.get(IClipboard::kText) == "preserved");
    destination.close();

    Clipboard valid;
    valid.open(0);
    valid.add(IClipboard::kText, "valid");
    valid.close();
    assert(destination.unmarshall(valid.marshall(), 2));
    destination.open(0);
    assert(destination.get(IClipboard::kText) == "valid");
    destination.close();

    ClipboardAssembler first, second;
    assert(first.accept(0, 1, kDataStart, "3", 10) == kStart);
    assert(second.accept(0, 2, kDataStart, "4", 10) == kStart);
    assert(first.accept(0, 1, kDataChunk, "abc", 10) == kNotFinish);
    assert(second.accept(0, 2, kDataChunk, "defg", 10) == kNotFinish);
    assert(first.accept(0, 1, kDataEnd, "", 10) == kFinish);
    assert(second.accept(0, 2, kDataEnd, "", 10) == kFinish);
    assert(first.data(0) == "abc" && second.data(0) == "defg");
    assert(first.accept(0, 3, kDataStart, "11", 10) == kError);
    assert(first.accept(0, 3, kDataStart, "2", 10) == kStart);
    assert(first.accept(0, 3, kDataChunk, "abc", 10) == kError);
    assert(first.accept(0, 3, kDataEnd, "", 10) == kError);
}
