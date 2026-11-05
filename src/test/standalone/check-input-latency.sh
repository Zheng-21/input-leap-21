#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."

# Minimal Linux smoke checks without Qt, X11 development libraries or Google Test.
# This config is only for these checks; it is not a production build configuration.
mkdir -p build/input-latency-check
printf '#pragma once\n' > build/input-latency-check/config.h
flags=(-std=c++17 -DSYSAPI_UNIX=1 -DHAVE_SYS_SOCKET_H=1
       -Ibuild/input-latency-check -Isrc/lib)
g++ "${flags[@]}" -fsyntax-only \
    src/lib/server/ClientConnectionByStream.cpp src/lib/server/ClientProxyUnknown.cpp \
    src/lib/client/ServerProxy.cpp src/lib/net/TCPSocket.cpp
g++ "${flags[@]}" -DNOLOGGING -ffunction-sections -fdata-sections \
    src/test/standalone/InputLatencyTests.cpp src/lib/server/ClientConnectionByStream.cpp \
    src/lib/base/EventTarget.cpp src/lib/inputleap/ProtocolUtil.cpp \
    src/lib/inputleap/protocol_types.cpp src/lib/base/XBase.cpp src/lib/base/String.cpp \
    -Wl,--gc-sections -o build/input-latency-check/sender-tests
build/input-latency-check/sender-tests
