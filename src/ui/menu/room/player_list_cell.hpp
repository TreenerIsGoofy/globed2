#pragma once
#include <defs.hpp>
#include <data/types/gd.hpp>

class PlayerListCell : public cocos2d::CCLayer {
public:
    static constexpr float CELL_HEIGHT = 45.0f;

    static PlayerListCell* create(const PlayerRoomPreviewAccountData& data);

protected:
    bool init(const PlayerRoomPreviewAccountData& data);
    void onOpenProfile(cocos2d::CCObject*);

    PlayerRoomPreviewAccountData data;

    cocos2d::CCMenu* menu;
    SimplePlayer* simplePlayer;
};