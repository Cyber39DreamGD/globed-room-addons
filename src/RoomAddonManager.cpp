#include "RoomAddonManager.hpp"

#include "Events.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GJAccountManager.hpp>
#include <globed/soft-link/API.hpp>

#include <chrono>
#include <utility>

using namespace geode::prelude;

namespace roomaddons {

RoomAddonManager& RoomAddonManager::get() {
    static RoomAddonManager instance;
    return instance;
}

int RoomAddonManager::ownAccountId() const {
    if (auto* am = GJAccountManager::get()) return am->m_accountID;
    return 0;
}

bool RoomAddonManager::isOwnerNow() const {
    return globed::api::available() && globed::api::room::isInRoom() && globed::api::room::isOwner();
}

bool RoomAddonManager::ownerSwitchOn() const {
    return Mod::get()->getSettingValue<bool>("room-addons-enabled");
}

void RoomAddonManager::setOwnerSwitch(bool on) {
    Mod::get()->setSettingValue("room-addons-enabled", on);
    broadcastAddonsState();
}

bool RoomAddonManager::addonsUnlocked() const {
    return m_state == VerifyState::Verified && ownerSwitchOn() && isOwnerNow();
}

void RoomAddonManager::onRosterUpdated(
    std::vector<std::pair<int, std::string>> players, uint32_t roomId, std::string roomName
) {
    std::map<int, std::string> next;
    for (auto& [id, name] : players) {
        if (id != 0) next[id] = name.empty() ? fmt::format("Player {}", id) : name;
    }

    if (int self = ownAccountId(); self != 0 && !next.contains(self)) {
        std::string name = "You";
        if (auto* am = GJAccountManager::get(); am && !am->m_username.empty()) {
            name = std::string(am->m_username.c_str());
        }
        next[self] = name;
    }

    bool changed = next != m_roster || roomId != m_roomId;
    m_roster = std::move(next);
    m_rosterKnown = true;
    m_roomId = roomId;
    if (!roomName.empty()) m_roomName = roomName;

    if (!changed) return;

    log::info("[Room Addons] room roster updated: {} player(s) in room {}", m_roster.size(), m_roomId);

    if (isOwnerNow()) {
        if (m_state == VerifyState::Verified) {
            m_ownerAddonsActive = false;
            broadcastAddonsState();
        }
        if (m_state == VerifyState::Verified || m_state == VerifyState::Missing) {
            m_state = VerifyState::Idle;
        }
        scheduleAutoCheck(1.0f);
    }
}

std::vector<RosterPlayer> RoomAddonManager::roster() const {
    std::vector<RosterPlayer> out;
    int self = ownAccountId();
    for (auto& [id, name] : m_roster) {
        out.push_back(RosterPlayer{
            .accountId = id,
            .username = name,
            .hasMod = id == self || m_replied.contains(id),
        });
    }
    return out;
}

std::vector<RosterPlayer> RoomAddonManager::missingPlayers() const {
    std::vector<RosterPlayer> out;
    int self = ownAccountId();
    for (auto& player : roster()) {
        if (player.accountId != self && !player.hasMod) out.push_back(player);
    }
    return out;
}

void RoomAddonManager::scheduleAutoCheck(float delaySeconds) {
    if (!Mod::get()->getSettingValue<bool>("auto-check")) return;
    if (!isOwnerNow()) return;
    m_autoCheckIn = delaySeconds;
}

void RoomAddonManager::startCheck() {
    if (!globed::api::available() || !globed::api::room::isInRoom()) {
        log::warn("[Room Addons] cannot check: not in a Globed room");
        return;
    }
    if (!globed::api::room::isOwner()) {
        log::warn("[Room Addons] cannot check: only the room owner runs the check");
        return;
    }

    m_roomId = globed::api::room::getId();
    m_replied.clear();

    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    m_nonce = static_cast<uint32_t>(now & 0x7fffffff) + 0x1000;

    PingEvent ping{m_nonce, m_roomId};
    globed::EventOptions opts{};
    opts.server = globed::EventServer::Central;
    opts.reliable = true;
    opts.urgent = true;
    int self = ownAccountId();
    for (auto& [id, _] : m_roster) {
        if (id != self && id != 0) opts.targetPlayers.push_back(id);
    }

    if (opts.targetPlayers.empty() && !m_rosterKnown) {
        for (int id : globed::api::game::getPlayerIds()) {
            if (id != self && id != 0) opts.targetPlayers.push_back(id);
        }
    }
    ping.send(opts);

    m_checkTimeLeft = static_cast<float>(Mod::get()->getSettingValue<int64_t>("check-timeout"));
    if (m_checkTimeLeft < 1.f) m_checkTimeLeft = 4.f;
    setState(VerifyState::Checking, false);
    log::info(
        "[Room Addons] hidden check sent (nonce {}, waiting on {} player(s))",
        m_nonce, opts.targetPlayers.size()
    );

    if (opts.targetPlayers.empty() && m_roster.size() <= 1) finishCheck();
}

void RoomAddonManager::finishCheck() {
    auto missing = missingPlayers();
    if (missing.empty()) {
        setState(VerifyState::Verified, true);
        log::info("[Room Addons] check passed: everyone in the room has the mod. Addons unlocked.");
    } else {
        setState(VerifyState::Missing, true);
        std::string names;
        for (auto& p : missing) names += (names.empty() ? "" : ", ") + p.username;
        log::warn("[Room Addons] check failed, addons OFF. Missing the mod: {}", names);
    }
    broadcastAddonsState();
}

void RoomAddonManager::setState(VerifyState next, bool notifyOwner) {
    bool changed = next != m_state;
    m_state = next;
    if (!changed || !notifyOwner || !isOwnerNow()) return;
    if (!Mod::get()->getSettingValue<bool>("owner-notifications")) return;

    if (next == VerifyState::Verified) {
        Notification::create("Room Addons unlocked: everyone has the mod!", NotificationIcon::Success, 3.f)->show();
    } else if (next == VerifyState::Missing) {
        Notification::create("Room Addons OFF: someone is missing the mod", NotificationIcon::Error, 3.f)->show();
    }
}

void RoomAddonManager::broadcastAddonsState() {
    if (!isOwnerNow()) return;
    AddonsStateEvent state{addonsUnlocked(), m_roomId};
    globed::EventOptions opts{};
    opts.server = globed::EventServer::Central;
    opts.reliable = true;
    state.send(opts);
    log::info("[Room Addons] told the room: addons are {}", addonsUnlocked() ? "ON" : "OFF");
}

void RoomAddonManager::tick(float dt) {
    if (!globed::api::available()) {
        m_state = VerifyState::GlobedUnavailable;
        m_ownerAddonsActive = false;
        return;
    }

    bool inRoom = globed::api::room::isInRoom();
    bool owner = inRoom && globed::api::room::isOwner();
    uint32_t roomId = inRoom ? globed::api::room::getId() : 0;

    if (roomId != m_lastRoomIdPolled || owner != m_lastOwnerPolled) {
        bool roomSwitched = roomId != m_lastRoomIdPolled;
        m_lastRoomIdPolled = roomId;
        m_lastOwnerPolled = owner;
        m_replied.clear();
        m_ownerAddonsActive = false;
        if (roomSwitched) {
            m_roster.clear();
            m_rosterKnown = false;
            m_roomName.clear();
            m_roomId = roomId;
        }
        if (!inRoom) {
            m_state = VerifyState::NotInRoom;
        } else if (!owner) {
            m_state = VerifyState::NotOwner;
        } else {
            m_state = VerifyState::Idle;
            scheduleAutoCheck(1.5f);
        }
        return;
    }

    if (!inRoom) {
        m_state = VerifyState::NotInRoom;
        return;
    }
    if (!owner) {
        m_state = VerifyState::NotOwner;
        return;
    }

    if (m_autoCheckIn >= 0.f) {
        m_autoCheckIn -= dt;
        if (m_autoCheckIn < 0.f && m_state != VerifyState::Checking) startCheck();
    }

    if (m_state == VerifyState::Checking) {
        m_checkTimeLeft -= dt;

        bool allReplied = true;
        int self = ownAccountId();
        for (auto& [id, _] : m_roster) {
            if (id != self && id != 0 && !m_replied.contains(id)) { allReplied = false; break; }
        }
        if ((allReplied && m_rosterKnown) || m_checkTimeLeft <= 0.f) finishCheck();
    }
}

void RoomAddonManager::handlePing(int senderId, uint32_t nonce, uint32_t roomId) {
    if (!globed::api::room::isInRoom()) return;
    if (senderId == 0 || senderId != globed::api::room::getOwner()) return;
    if (roomId != globed::api::room::getId()) return;
    if (senderId == ownAccountId()) return;

    PongEvent pong{nonce, roomId};
    globed::EventOptions opts{};
    opts.server = globed::EventServer::Central;
    opts.reliable = true;
    opts.urgent = true;
    opts.targetPlayers.push_back(senderId);
    pong.send(opts);
    log::debug("[Room Addons] silently answered the owner's hidden check");
}

void RoomAddonManager::handlePong(int senderId, uint32_t nonce, uint32_t roomId) {
    if (m_state != VerifyState::Checking) return;
    if (!isOwnerNow() || roomId != m_roomId || nonce != m_nonce) return;
    if (senderId == 0 || !m_roster.contains(senderId)) return;
    m_replied.insert(senderId);
    log::debug("[Room Addons] hidden reply from {} ({}/{})", senderId, m_replied.size(), m_roster.size());
}

void RoomAddonManager::handleAddonsState(int senderId, bool enabled, uint32_t roomId) {
    if (!globed::api::room::isInRoom()) return;
    if (senderId == 0 || senderId != globed::api::room::getOwner()) return;
    if (roomId != globed::api::room::getId()) return;
    m_ownerAddonsActive = enabled;
    log::info("[Room Addons] owner turned room addons {}", enabled ? "ON" : "OFF");
}

void RoomAddonManager::handleAnnouncement(int senderId, const std::string& message) {
    if (!globed::api::room::isInRoom()) return;
    if (senderId == 0 || senderId != globed::api::room::getOwner()) return;
    if (senderId == ownAccountId()) return;

    Notification::create(fmt::format("Room owner: {}", message), NotificationIcon::Info, 4.f)->show();
}

void RoomAddonManager::handleSyncRestart(int senderId, uint32_t nonce, uint32_t roomId) {
    if (!globed::api::room::isInRoom()) return;
    if (senderId == 0 || senderId != globed::api::room::getOwner()) return;
    if (roomId != globed::api::room::getId() || nonce == m_lastSyncNonce) return;
    if (senderId == ownAccountId()) return;
    m_lastSyncNonce = nonce;
    if (globed::api::game::isActive()) {
        globed::api::game::causeLocalRespawn(true);
        log::info("[Room Addons] sync restart from the room owner");
    }
}

void RoomAddonManager::sendAnnouncement(const std::string& message) {
    if (!addonsUnlocked() || message.empty()) return;
    AnnouncementEvent event{message};
    globed::EventOptions opts{};
    opts.server = globed::EventServer::Central;
    opts.reliable = true;
    event.send(opts);
    Notification::create("Announcement sent to the room", NotificationIcon::Success, 2.f)->show();
}

void RoomAddonManager::sendSyncRestart() {
    if (!addonsUnlocked()) return;
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    uint32_t nonce = static_cast<uint32_t>(now & 0x7fffffff) + 0x2000;
    m_lastSyncNonce = nonce;
    SyncRestartEvent event{nonce, m_roomId};
    globed::EventOptions opts{};
    opts.server = globed::EventServer::Central;
    opts.reliable = true;
    opts.urgent = true;
    event.send(opts);

    if (globed::api::game::isActive()) globed::api::game::causeLocalRespawn(true);
    Notification::create("Sync restart sent!", NotificationIcon::Success, 2.f)->show();
}

std::string RoomAddonManager::statusText() const {
    switch (m_state) {
        case VerifyState::GlobedUnavailable: return "Globed is not available.";
        case VerifyState::NotInRoom: return "Join or create a Globed room first.";
        case VerifyState::NotOwner: return "Only the room owner can use Room Addons.";
        case VerifyState::Idle: return "Not checked yet. Press Check Room.";
        case VerifyState::Checking: return "Checking who has the mod...";
        case VerifyState::Verified:
            return ownerSwitchOn()
                ? "Everyone has the mod - Room Addons are ON!"
                : "Everyone has the mod, but your Room Addons switch is OFF.";
        case VerifyState::Missing: {
            auto missing = missingPlayers();
            return fmt::format(
                "{} player(s) don't have the mod - Room Addons are OFF.", missing.size()
            );
        }
    }
    return "";
}

std::string RoomAddonManager::inviteText() {
    return "You need to install my mod to get more mods for Globed and more fun! "
           "Open Geode, search for \"Globed Room Addons\" "
           "(cyber39dreamgd.globed-room-addons) and install it, then rejoin the room.";
}

}
