#include "OwnerPanel.hpp"

#include "RoomAddonManager.hpp"

#include <globed/soft-link/API.hpp>

using namespace geode::prelude;
using namespace roomaddons;

OwnerPanel* OwnerPanel::create() {
    auto* ret = new OwnerPanel();
    if (ret && ret->init(460.f, 320.f)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool OwnerPanel::init(float width, float height) {
    if (!Popup::init(width, height)) return false;

    this->setTitle("Globed Room Addons");
    this->scheduleUpdate();

    auto& mgr = RoomAddonManager::get();

    m_statusLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_statusLabel->setScale(0.42f);
    m_mainLayer->addChildAtPosition(m_statusLabel, Anchor::Top, {0.f, -50.f});

    m_playersLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_playersLabel->setScale(0.7f);
    m_playersLabel->setAnchorPoint({0.f, 1.f});
    m_mainLayer->addChildAtPosition(m_playersLabel, Anchor::TopLeft, {24.f, -68.f});

    auto* checkBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Check Room", "goldFont.fnt", "GJ_button_01.png", 0.7f),
        this, menu_selector(OwnerPanel::onCheck)
    );
    m_buttonMenu->addChildAtPosition(checkBtn, Anchor::BottomLeft, {76.f, 100.f});

    auto* copyBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Copy Invite", "goldFont.fnt", "GJ_button_04.png", 0.7f),
        this, menu_selector(OwnerPanel::onCopyInvite)
    );
    m_buttonMenu->addChildAtPosition(copyBtn, Anchor::BottomLeft, {76.f, 62.f});

    auto* switchLabel = CCLabelBMFont::create("Addons:", "bigFont.fnt");
    switchLabel->setScale(0.42f);
    switchLabel->setAnchorPoint({1.f, 0.5f});
    m_mainLayer->addChildAtPosition(switchLabel, Anchor::BottomRight, {-62.f, 100.f});

    m_addonsToggle = CCMenuItemToggler::create(
        CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png"),
        CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png"),
        this, menu_selector(OwnerPanel::onToggleAddons)
    );
    m_addonsToggle->setScale(0.75f);
    m_addonsToggle->toggle(mgr.ownerSwitchOn());
    m_buttonMenu->addChildAtPosition(m_addonsToggle, Anchor::BottomRight, {-30.f, 100.f});

    m_syncBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Sync Restart", "goldFont.fnt", "GJ_button_02.png", 0.7f),
        this, menu_selector(OwnerPanel::onSyncRestart)
    );
    m_buttonMenu->addChildAtPosition(m_syncBtn, Anchor::Bottom, {10.f, 100.f});

    m_announceInput = TextInput::create(190.f, "Announcement...", "bigFont.fnt");
    m_announceInput->setMaxCharCount(80);
    m_announceInput->setScale(0.85f);
    m_mainLayer->addChildAtPosition(m_announceInput, Anchor::BottomRight, {-78.f, 62.f});

    m_announceBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Send", "goldFont.fnt", "GJ_button_01.png", 0.7f),
        this, menu_selector(OwnerPanel::onSendAnnouncement)
    );
    m_buttonMenu->addChildAtPosition(m_announceBtn, Anchor::BottomRight, {-30.f, 62.f});

    refresh();
    return true;
}

void OwnerPanel::update(float dt) {
    Popup::update(dt);
    m_refreshTimer += dt;
    if (m_refreshTimer >= 0.4f) {
        m_refreshTimer = 0.f;
        refresh();
    }
}

void OwnerPanel::refresh() {
    auto& mgr = RoomAddonManager::get();
    if (m_statusLabel) m_statusLabel->setString(mgr.statusText().c_str());

    if (m_playersLabel) {
        std::string text;
        if (!globed::api::room::isInRoom()) {
            text = "You are not in a Globed room.";
        } else if (!globed::api::room::isOwner()) {
            text = "You are not the owner of this room.\nRoom Addons are owner-only.";
        } else {
            auto players = mgr.roster();
            text = fmt::format("Room players ({}) - who has the mod:\n", players.size());
            size_t shown = 0;
            for (auto& p : players) {
                if (shown >= 7) {
                    text += fmt::format("... and {} more\n", players.size() - shown);
                    break;
                }
                text += fmt::format("{} {}\n", p.hasMod ? "[YES]" : "[NO ]", p.username);
                ++shown;
            }
            if (mgr.state() == VerifyState::Missing) {
                text += "\nPlayers with [NO] need the mod installed\nbefore Room Addons can turn on.";
            }
        }
        m_playersLabel->setString(text.c_str());
    }

    bool unlocked = mgr.addonsUnlocked();
    for (auto* btn : {m_announceBtn, m_syncBtn}) {
        if (!btn) continue;
        btn->setEnabled(unlocked);
        btn->setColor(unlocked ? ccColor3B{255, 255, 255} : ccColor3B{110, 110, 110});
    }
    if (m_addonsToggle) {
        bool on = mgr.ownerSwitchOn();
        if (m_addonsToggle->isToggled() != on) m_addonsToggle->toggle(on);
    }
}

void OwnerPanel::onCheck(CCObject*) {
    RoomAddonManager::get().startCheck();
    refresh();
}

void OwnerPanel::onCopyInvite(CCObject*) {
    if (utils::clipboard::write(RoomAddonManager::inviteText())) {
        Notification::create("Invite copied! Paste it to players missing the mod.", NotificationIcon::Success, 3.f)->show();
    } else {
        FLAlertLayer::create("Invite", RoomAddonManager::inviteText().c_str(), "OK")->show();
    }
}

void OwnerPanel::onToggleAddons(CCObject* sender) {
    auto* toggler = static_cast<CCMenuItemToggler*>(sender);
    RoomAddonManager::get().setOwnerSwitch(toggler->isToggled());
    refresh();
}

void OwnerPanel::onSendAnnouncement(CCObject*) {
    if (!m_announceInput) return;
    RoomAddonManager::get().sendAnnouncement(std::string(m_announceInput->getString().c_str()));
    m_announceInput->setString("");
}

void OwnerPanel::onSyncRestart(CCObject*) {
    RoomAddonManager::get().sendSyncRestart();
}
