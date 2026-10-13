#pragma once

#include <Geode/Geode.hpp>

class OwnerPanel : public geode::Popup {
public:
    static OwnerPanel* create();

protected:
    bool init(float width, float height);
    void update(float dt) override;

private:
    void refresh();
    void onCheck(CCObject*);
    void onCopyInvite(CCObject*);
    void onToggleAddons(CCObject*);
    void onSendAnnouncement(CCObject*);
    void onSyncRestart(CCObject*);

    CCLabelBMFont* m_statusLabel = nullptr;
    CCLabelBMFont* m_playersLabel = nullptr;
    TextInput* m_announceInput = nullptr;
    CCMenuItemSpriteExtra* m_announceBtn = nullptr;
    CCMenuItemSpriteExtra* m_syncBtn = nullptr;
    CCMenuItemToggler* m_addonsToggle = nullptr;
    float m_refreshTimer = 0.f;
};
