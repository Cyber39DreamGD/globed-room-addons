#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

namespace roomaddons {

enum class VerifyState {
    GlobedUnavailable,
    NotInRoom,
    NotOwner,
    Idle,
    Checking,
    Verified,
    Missing,
};

struct RosterPlayer {
    int accountId = 0;
    std::string username;
    bool hasMod = false;
};

class RoomAddonManager {
public:
    static RoomAddonManager& get();

    void tick(float dt);

    void onRosterUpdated(std::vector<std::pair<int, std::string>> players, uint32_t roomId, std::string roomName);
    std::vector<RosterPlayer> roster() const;
    bool rosterKnown() const { return m_rosterKnown; }
    std::string roomName() const { return m_roomName; }

    void startCheck();
    VerifyState state() const { return m_state; }
    std::vector<RosterPlayer> missingPlayers() const;
    std::string statusText() const;

    bool ownerSwitchOn() const;
    void setOwnerSwitch(bool on);

    bool addonsUnlocked() const;

    bool ownerAddonsActive() const { return m_ownerAddonsActive; }

    void sendAnnouncement(const std::string& message);
    void sendSyncRestart();

    static std::string inviteText();

    void handlePing(int senderId, uint32_t nonce, uint32_t roomId);
    void handlePong(int senderId, uint32_t nonce, uint32_t roomId);
    void handleAddonsState(int senderId, bool enabled, uint32_t roomId);
    void handleAnnouncement(int senderId, const std::string& message);
    void handleSyncRestart(int senderId, uint32_t nonce, uint32_t roomId);

private:
    RoomAddonManager() = default;

    bool isOwnerNow() const;
    int ownAccountId() const;
    void setState(VerifyState next, bool notifyOwner);
    void finishCheck();
    void broadcastAddonsState();
    void scheduleAutoCheck(float delaySeconds);

    VerifyState m_state = VerifyState::NotInRoom;
    std::map<int, std::string> m_roster;
    bool m_rosterKnown = false;
    std::string m_roomName;
    uint32_t m_roomId = 0;

    uint32_t m_nonce = 0;
    std::unordered_set<int> m_replied;
    float m_checkTimeLeft = 0.f;

    float m_autoCheckIn = -1.f;
    uint32_t m_lastRoomIdPolled = 0;
    bool m_lastOwnerPolled = false;

    bool m_ownerAddonsActive = false;
    uint32_t m_lastSyncNonce = 0;
};

}
