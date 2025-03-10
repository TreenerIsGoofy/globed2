#pragma once
#include <defs.hpp>

#include <Geode/modify/PlayLayer.hpp>

#include <game/interpolator.hpp>
#include <game/player_store.hpp>
#include <net/network_manager.hpp>
#include <ui/game/player/remote_player.hpp>
#include <ui/game/overlay/overlay.hpp>
#include <ui/game/progress/progress_icon.hpp>
#include <ui/game/progress/progress_arrow.hpp>

float adjustLerpTimeDelta(float dt);

class $modify(GlobedPlayLayer, PlayLayer) {
    // setup stuff
    bool globedReady = false;
    uint32_t configuredTps = 0;

    // in game stuff
    bool deafened = false;
    uint32_t totalSentPackets = 0;
    float timeCounter = 0.f;
    float lastServerUpdate = 0.f;
    std::shared_ptr<PlayerInterpolator> interpolator;
    std::shared_ptr<PlayerStore> playerStore;

    bool isCurrentlyDead = false;
    std::optional<SpiderTeleportData> spiderTp1, spiderTp2;
    float lastDeathTimestamp = 0.f;

    // ui elements
    GlobedOverlay* overlay = nullptr;
    std::unordered_map<int, RemotePlayer*> players;
    Ref<PlayerProgressIcon> selfProgressIcon = nullptr;
    Ref<CCNode> progressBarWrapper = nullptr;
    Ref<PlayerStatusIcons> selfStatusIcons = nullptr;

    // speedhack detection
    float lastKnownTimeScale = 1.0f;
    std::unordered_map<int, util::time::time_point> lastSentPacket;

    // gd hooks

    bool init(GJGameLevel* level, bool p1, bool p2);
    void onQuit();

    /* setup stuff to make init() cleaner */

    void setupPacketListeners();
    void setupCustomKeybinds();

    /* periodical selectors */

    // selSendPlayerData - runs tps (default 30) times per second
    void selSendPlayerData(float);

    // selPeriodicalUpdate - runs 4 times a second, does various stuff
    void selPeriodicalUpdate(float);

    // selUpdate - runs every frame, increments the non-decreasing time counter, interpolates and updates players
    void selUpdate(float dt);

    bool established() {
        // the 2nd check is in case we disconnect while being in a level somehow
        return m_fields->globedReady && NetworkManager::get().established();
    }

    bool isCurrentPlayLayer() {
        auto playLayer = geode::cocos::getChildOfType<PlayLayer>(cocos2d::CCScene::get(), 0);
        return playLayer == this;
    }

    bool isPaused() {
        if (!isCurrentPlayLayer()) return false;

        for (CCNode* child : CCArrayExt<CCNode*>(this->getParent()->getChildren())) {
            if (typeinfo_cast<PauseLayer*>(child)) {
                return true;
            }
        }

        return false;
    }

    bool shouldLetMessageThrough(int playerId);

    SpecificIconData gatherSpecificIconData(PlayerObject* player);
    PlayerData gatherPlayerData();

    void handlePlayerJoin(int playerId);
    void handlePlayerLeave(int playerId);

    // With speedhack enabled, all scheduled selectors will run more often than they are supposed to.
    // This means, if you turn up speedhack to let's say 100x, you will send 3000 packets per second. That is a big no-no.
    // For naive speedhack implementations, we simply check CCScheduler::getTimeScale and properly reschedule our data sender.
    //
    // For non-naive speedhacks however, ones that don't use CCScheduler::setTimeScale, it is more complicated.
    // We record the time of sending each packet and compare the intervals. If the interval is suspiciously small, we reject the packet.
    // This does result in less smooth experience with non-naive speedhacks however.
    bool accountForSpeedhack(size_t uniqueKey, float cap, float allowance = 0.9f);

    void unscheduleSelectors();
    void rescheduleSelectors();
};