#pragma once
#include <defs.hpp>

#include "signup_layer.hpp"
#include <Geode/utils/web.hpp>

class GlobedMenuLayer : public cocos2d::CCLayer {
public:
    static constexpr float LIST_WIDTH = 358.f;
    static constexpr float LIST_HEIGHT = 220.f;

    ~GlobedMenuLayer();

    static GlobedMenuLayer* create();
    static cocos2d::CCScene* scene();

private:
    GJListLayer* listLayer;
    GlobedSignupLayer* signupLayer;
    std::optional<geode::utils::web::SentAsyncWebRequestHandle> serverRequestHandle;
    cocos2d::CCSequence* timeoutSequence;

    bool init();
    cocos2d::CCArray* createServerList();
    void refreshServerList(float dt);
    void requestServerList();
    void keyBackClicked();
    void pingServers(float dt);

    void cancelWebRequest();
};