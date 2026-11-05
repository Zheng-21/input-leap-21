/* InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "server/ClientConnectionByStream.h"
#include "io/IStream.h"
#include "base/IEventQueueBuffer.h"
#include <cassert>
#include <iostream>
#include <map>
#include <queue>
#include <vector>

using namespace inputleap;

class Queue : public IEventQueue {
public:
    using Key = std::pair<EventType, const EventTarget*>;
    std::map<Key, EventHandler> handlers;
    std::queue<Event> pending;
    void loop() override {
        while (!pending.empty()) {
            auto event = std::move(pending.front());
            pending.pop();
            dispatchEvent(event);
        }
    }
    void set_buffer(std::unique_ptr<IEventQueueBuffer>) override {}
    bool getEvent(Event&, double) override { return false; }
    bool dispatchEvent(const Event& event) override {
        const auto it = handlers.find({event.getType(), event.getTarget()});
        if (it == handlers.end()) { return false; }
        auto handler = it->second;
        handler(event);
        return true;
    }
    void add_event(Event&& event) override { pending.push(std::move(event)); }
    EventQueueTimer* newTimer(double, const EventTarget*) override { return nullptr; }
    EventQueueTimer* newOneShotTimer(double, const EventTarget*) override { return nullptr; }
    void deleteTimer(EventQueueTimer*) override {}
    void add_handler(EventType type, const EventTarget* target, const EventHandler& h) override {
        handlers[{type, target}] = h;
    }
    void remove_handler(EventType type, const EventTarget* target) override {
        handlers.erase({type, target});
    }
    void remove_handlers(const EventTarget*) override {}
    void waitForReady() const override {}
    const EventTarget* getSystemTarget() override { return nullptr; }
};

class Stream : public IStream {
public:
    std::vector<std::string> packets;
    EventTarget target;
    bool flushed = false, closed = false;
    void close() override { closed = true; }
    std::uint32_t read(void*, std::uint32_t) override { return 0; }
    void write(const void* data, std::uint32_t n) override {
        assert(!closed);
        packets.emplace_back(static_cast<const char*>(data), n);
    }
    void flush() override { flushed = true; }
    void shutdownInput() override {}
    void shutdownOutput() override {}
    const EventTarget* get_event_target() const override { return &target; }
    bool isReady() const override { return false; }
    std::uint32_t getSize() const override { return 0; }
};

int coordinate(const std::string& packet, unsigned offset) {
    unsigned value = (static_cast<unsigned char>(packet.at(offset)) << 8) |
                     static_cast<unsigned char>(packet.at(offset + 1));
    return value >= 32768 ? static_cast<int>(value) - 65536 : value;
}

int main() {
    Queue queue;
    auto owned = std::make_unique<Stream>();
    auto& stream = *owned;
    ClientConnectionByStream conn(std::move(owned), &queue);

    // A burst emits only the latest absolute position, including the final event.
    for (int i = 0; i < 1000; ++i) { conn.send_mouse_move_1_6(i, i + 1); }
    assert(stream.packets.empty() && queue.pending.size() == 1);
    queue.loop();
    assert(stream.packets.size() == 1);
    assert(coordinate(stream.packets[0], 4) == 999);
    assert(coordinate(stream.packets[0], 6) == 1000);

    // Movement before a click/key/wheel must reach the peer first.
    stream.packets.clear();
    conn.send_mouse_move_1_6(10, 20);
    conn.send_mouse_down_1_6(1);
    conn.send_mouse_move_1_6(30, 40);
    conn.send_mouse_up_1_6(1);
    conn.send_mouse_move_1_6(50, 60);
    conn.send_key_down_1_6(65, 0, 1);
    conn.send_mouse_move_1_6(70, 80);
    conn.send_mouse_wheel_1_6(0, 120);
    queue.loop();
    assert(stream.packets.size() == 8);
    assert(stream.packets[1].substr(0, 4) == "DMDN");
    assert(stream.packets[3].substr(0, 4) == "DMUP");
    assert(stream.packets[5].substr(0, 4) == "DKDN");
    assert(stream.packets[7].substr(0, 4) == "DMWM");

    stream.packets.clear();
    conn.send_mouse_relative_move_1_6(100, -200);
    conn.send_mouse_relative_move_1_6(300, -400);
    queue.loop();
    assert(stream.packets.size() == 1);
    assert(coordinate(stream.packets[0], 4) == 400);
    assert(coordinate(stream.packets[0], 6) == -600);

    // Neither positive nor negative accumulation may wrap the signed wire type.
    stream.packets.clear();
    conn.send_mouse_relative_move_1_6(30000, -30000);
    conn.send_mouse_relative_move_1_6(10000, -10000);
    queue.loop();
    assert(stream.packets.size() == 2);
    assert(coordinate(stream.packets[0], 4) + coordinate(stream.packets[1], 4) == 40000);
    assert(coordinate(stream.packets[0], 6) + coordinate(stream.packets[1], 6) == -40000);

    stream.packets.clear();
    conn.send_mouse_move_1_6(5, 6);
    conn.send_mouse_relative_move_1_6(7, 8);
    conn.send_mouse_move_1_6(9, 10);
    conn.flush();
    queue.loop();
    assert(stream.packets.size() == 3 && stream.flushed);
    assert(coordinate(stream.packets[0], 4) == 5);
    assert(coordinate(stream.packets[1], 4) == 7);
    assert(coordinate(stream.packets[2], 4) == 9);

    conn.send_mouse_move_1_6(11, 12);
    conn.close();
    queue.loop();
    assert(stream.closed && stream.packets.size() == 3);
    {
        auto another = std::make_unique<Stream>();
        ClientConnectionByStream temporary(std::move(another), &queue);
        temporary.send_mouse_move_1_6(1, 2);
    }
    queue.loop(); // A queued flush after destruction must not invoke a dangling callback.
    assert(queue.handlers.empty());
    std::cout << "Input latency sender regression checks passed\n";
}
