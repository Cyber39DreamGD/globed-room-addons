#pragma once

#include <Geode/Geode.hpp>
#include <globed/core/Event.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace roomaddons {

inline void putU32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

inline bool getU32(std::span<const uint8_t> data, size_t& offset, uint32_t& out) {
    if (offset + 4 > data.size()) return false;
    out = static_cast<uint32_t>(data[offset])
        | (static_cast<uint32_t>(data[offset + 1]) << 8)
        | (static_cast<uint32_t>(data[offset + 2]) << 16)
        | (static_cast<uint32_t>(data[offset + 3]) << 24);
    offset += 4;
    return true;
}

inline constexpr uint32_t PROTOCOL_VERSION = 1;

struct PingEvent : globed::ServerEvent<PingEvent, globed::EventServer::Central> {
    static constexpr auto Id = "room-addon-ping"_spr;

    uint32_t nonce = 0;
    uint32_t roomId = 0;

    PingEvent() = default;
    PingEvent(uint32_t nonce_, uint32_t roomId_) : nonce(nonce_), roomId(roomId_) {}

    std::vector<uint8_t> encode() const {
        std::vector<uint8_t> out;
        putU32(out, PROTOCOL_VERSION);
        putU32(out, nonce);
        putU32(out, roomId);
        return out;
    }

    static geode::Result<PingEvent> decode(std::span<const uint8_t> data) {
        size_t offset = 0;
        uint32_t version = 0;
        PingEvent out;
        if (!getU32(data, offset, version) || version != PROTOCOL_VERSION) {
            return geode::Err("bad ping version");
        }
        if (!getU32(data, offset, out.nonce) || !getU32(data, offset, out.roomId)) {
            return geode::Err("bad ping");
        }
        return geode::Ok(out);
    }
};

struct PongEvent : globed::ServerEvent<PongEvent, globed::EventServer::Central> {
    static constexpr auto Id = "room-addon-pong"_spr;

    uint32_t nonce = 0;
    uint32_t roomId = 0;

    PongEvent() = default;
    PongEvent(uint32_t nonce_, uint32_t roomId_) : nonce(nonce_), roomId(roomId_) {}

    std::vector<uint8_t> encode() const {
        std::vector<uint8_t> out;
        putU32(out, PROTOCOL_VERSION);
        putU32(out, nonce);
        putU32(out, roomId);
        return out;
    }

    static geode::Result<PongEvent> decode(std::span<const uint8_t> data) {
        size_t offset = 0;
        uint32_t version = 0;
        PongEvent out;
        if (!getU32(data, offset, version) || version != PROTOCOL_VERSION) {
            return geode::Err("bad pong version");
        }
        if (!getU32(data, offset, out.nonce) || !getU32(data, offset, out.roomId)) {
            return geode::Err("bad pong");
        }
        return geode::Ok(out);
    }
};

struct AddonsStateEvent : globed::ServerEvent<AddonsStateEvent, globed::EventServer::Central> {
    static constexpr auto Id = "room-addon-state"_spr;

    bool enabled = false;
    uint32_t roomId = 0;

    AddonsStateEvent() = default;
    AddonsStateEvent(bool enabled_, uint32_t roomId_) : enabled(enabled_), roomId(roomId_) {}

    std::vector<uint8_t> encode() const {
        std::vector<uint8_t> out;
        putU32(out, PROTOCOL_VERSION);
        out.push_back(enabled ? 1 : 0);
        putU32(out, roomId);
        return out;
    }

    static geode::Result<AddonsStateEvent> decode(std::span<const uint8_t> data) {
        size_t offset = 0;
        uint32_t version = 0;
        AddonsStateEvent out;
        if (!getU32(data, offset, version) || version != PROTOCOL_VERSION) {
            return geode::Err("bad state version");
        }
        if (offset >= data.size()) return geode::Err("bad state");
        out.enabled = data[offset++] != 0;
        if (!getU32(data, offset, out.roomId)) return geode::Err("bad state room");
        return geode::Ok(out);
    }
};

struct AnnouncementEvent : globed::ServerEvent<AnnouncementEvent, globed::EventServer::Central> {
    static constexpr auto Id = "room-addon-announcement"_spr;

    std::string message;

    AnnouncementEvent() = default;
    AnnouncementEvent(std::string message_) : message(std::move(message_)) {}

    std::vector<uint8_t> encode() const {
        auto text = message.substr(0, 200);
        return {text.begin(), text.end()};
    }

    static geode::Result<AnnouncementEvent> decode(std::span<const uint8_t> data) {
        if (data.empty() || data.size() > 512) return geode::Err("bad announcement");
        return geode::Ok(AnnouncementEvent{
            std::string{reinterpret_cast<const char*>(data.data()), data.size()}});
    }
};

struct SyncRestartEvent : globed::ServerEvent<SyncRestartEvent, globed::EventServer::Central> {
    static constexpr auto Id = "room-addon-sync-restart"_spr;

    uint32_t nonce = 0;
    uint32_t roomId = 0;

    SyncRestartEvent() = default;
    SyncRestartEvent(uint32_t nonce_, uint32_t roomId_) : nonce(nonce_), roomId(roomId_) {}

    std::vector<uint8_t> encode() const {
        std::vector<uint8_t> out;
        putU32(out, PROTOCOL_VERSION);
        putU32(out, nonce);
        putU32(out, roomId);
        return out;
    }

    static geode::Result<SyncRestartEvent> decode(std::span<const uint8_t> data) {
        size_t offset = 0;
        uint32_t version = 0;
        SyncRestartEvent out;
        if (!getU32(data, offset, version) || version != PROTOCOL_VERSION) {
            return geode::Err("bad sync version");
        }
        if (!getU32(data, offset, out.nonce) || !getU32(data, offset, out.roomId)) {
            return geode::Err("bad sync");
        }
        return geode::Ok(out);
    }
};

}
