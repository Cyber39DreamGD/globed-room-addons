#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include <globed/soft-link/API.hpp>
#include <globed/core/Event.hpp>
#include <globed/core/data/Messages.hpp>
#include <globed/core/net/MessageListener.hpp>

#include "Events.hpp"
#include "OwnerPanel.hpp"
#include "RoomAddonManager.hpp"

using namespace geode::prelude;
using namespace roomaddons;

class RoomAddonTickNode : public CCNode {
public:
    static RoomAddonTickNode* get() {
        static auto* node = [] {
            auto* n = new RoomAddonTickNode();
            n->retain();
            CCDirector::get()->getScheduler()->scheduleSelector(
                schedule_selector(RoomAddonTickNode::tick), n, 0.25f, false
            );
            return n;
        }();
        return node;
    }

    void tick(float dt) {
        RoomAddonManager::get().tick(dt);
        tryInjectGlobedButton();
        updateMenuLayerButton();
    }

    void onOwnerButton(CCObject*) {
        if (globed::api::room::isInRoom() && globed::api::room::isOwner()) {
            if (auto* panel = OwnerPanel::create()) panel->show();
        }
    }

private:
    void tryInjectGlobedButton() {
        auto* scene = CCDirector::get()->getRunningScene();
        if (!scene) return;

        bool shouldShow = globed::api::available()
            && globed::api::room::isInRoom()
            && globed::api::room::isOwner();

        if (auto* existing = scene->getChildByIDRecursive("room-addons-btn"_spr)) {
            existing->setVisible(shouldShow);
            return;
        }
        if (!shouldShow) return;

        auto* menuNode = scene->getChildByIDRecursive("right-side-menu");
        auto* menu = typeinfo_cast<CCMenu*>(menuNode);
        if (!menu) return;

        CCNode* sprite = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
        if (!sprite) sprite = ButtonSprite::create("Addons");
        sprite->setScale(0.75f);

        auto* btn = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(RoomAddonTickNode::onOwnerButton));
        btn->setID("room-addons-btn"_spr);
        menu->addChild(btn);
        menu->updateLayout();
    }

    void updateMenuLayerButton() {
        auto* scene = CCDirector::get()->getRunningScene();
        if (!scene) return;
        if (auto* btn = scene->getChildByIDRecursive("room-addons-menu-btn"_spr)) {
            btn->setVisible(
                globed::api::available()
                && globed::api::room::isInRoom()
                && globed::api::room::isOwner()
            );
        }
    }
};

class $modify(RoomAddonsMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto* sprite = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
        if (!sprite) return true;
        auto* btn = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(RoomAddonsMenuLayer::onRoomAddons)
        );
        btn->setID("room-addons-menu-btn"_spr);
        btn->setVisible(false);
        if (auto* menu = this->getChildByID("bottom-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        }
        return true;
    }

    void onRoomAddons(CCObject*) {
        if (globed::api::room::isInRoom() && globed::api::room::isOwner()) {
            if (auto* panel = OwnerPanel::create()) panel->show();
        }
    }
};

$on_mod(Loaded) {
    (void)RoomAddonTickNode::get();
    auto& mgr = RoomAddonManager::get();

    PingEvent::listen([](PingEvent const& event, globed::EventOptions const& opts) {
        RoomAddonManager::get().handlePing(opts.sender, event.nonce, event.roomId);
    }).leak();

    PongEvent::listen([](PongEvent const& event, globed::EventOptions const& opts) {
        RoomAddonManager::get().handlePong(opts.sender, event.nonce, event.roomId);
    }).leak();

    AddonsStateEvent::listen([](AddonsStateEvent const& event, globed::EventOptions const& opts) {
        RoomAddonManager::get().handleAddonsState(opts.sender, event.enabled, event.roomId);
    }).leak();

    AnnouncementEvent::listen([](AnnouncementEvent const& event, globed::EventOptions const& opts) {
        RoomAddonManager::get().handleAnnouncement(opts.sender, event.message);
    }).leak();

    SyncRestartEvent::listen([](SyncRestartEvent const& event, globed::EventOptions const& opts) {
        RoomAddonManager::get().handleSyncRestart(opts.sender, event.nonce, event.roomId);
    }).leak();

    globed::MessageEvent<globed::msg::RoomStateMessage>{false}.listen(
        [](globed::msg::RoomStateMessage const& msg) {
            std::vector<std::pair<int, std::string>> players;
            for (auto const& p : msg.players) {
                players.emplace_back(p.accountData.accountId, p.accountData.username);
            }
            RoomAddonManager::get().onRosterUpdated(players, msg.roomId, msg.roomName);
        }
    ).leak();

    globed::MessageEvent<globed::msg::RoomPlayersMessage>{false}.listen(
        [](globed::msg::RoomPlayersMessage const& msg) {
            std::vector<std::pair<int, std::string>> players;
            for (auto const& p : msg.players) {
                players.emplace_back(p.accountData.accountId, p.accountData.username);
            }
            uint32_t roomId = globed::api::room::isInRoom() ? globed::api::room::getId() : 0;
            RoomAddonManager::get().onRosterUpdated(players, roomId, "");
        }
    ).leak();

    KeybindSettingPressedEventV3(Mod::get(), "open-panel-kb").listen(
        [](Keybind const&, bool down, bool repeat, double) {
            if (!down || repeat) return;
            if (globed::api::room::isInRoom() && globed::api::room::isOwner()) {
                if (auto* panel = OwnerPanel::create()) panel->show();
            }
        }
    ).leak();

    KeybindSettingPressedEventV3(Mod::get(), "sync-restart-kb").listen(
        [](Keybind const&, bool down, bool repeat, double) {
            if (!down || repeat) return;
            RoomAddonManager::get().sendSyncRestart();
        }
    ).leak();

    log::info("[Room Addons] loaded. Waiting for a Globed room where you are the owner.");
    (void)mgr;
}
