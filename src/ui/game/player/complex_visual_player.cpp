#include "complex_visual_player.hpp"

#include "remote_player.hpp"
#include <managers/settings.hpp>
#include <util/misc.hpp>

using namespace geode::prelude;

bool ComplexVisualPlayer::init(RemotePlayer* parent, bool isSecond) {
    if (!CCNode::init() || !BaseVisualPlayer::init(parent, isSecond)) return false;

    this->playLayer = PlayLayer::get();

    auto& data = parent->getAccountData();

    auto& settings = GlobedSettings::get();

    playerIcon = static_cast<ComplexPlayerObject*>(Build<PlayerObject>::create(1, 1, this->playLayer, this->playLayer->m_objectLayer, false)
        .opacity(static_cast<unsigned char>(settings.players.playerOpacity * 255.f))
        .parent(this)
        .collect());

    playerIcon->setRemoteState();

    Build<CCLabelBMFont>::create(data.name.c_str(), "chatFont.fnt")
        .opacity(static_cast<unsigned char>(settings.players.nameOpacity * 255.f))
        .visible(settings.players.showNames && (!isSecond || settings.players.dualName))
        .store(playerName)
        .pos(0.f, 25.f)
        .parent(this);

    this->updateIcons(data.icons);

    if (!isSecond && settings.players.statusIcons) {
        statusIcons = Build<PlayerStatusIcons>::create()
            .scale(0.8f)
            .anchorPoint(0.5f, 0.f)
            .pos(0.f, settings.players.showNames ? 40.f : 25.f)
            .parent(this)
            .id("status-icons"_spr)
            .collect();
    }

    return true;
}

void ComplexVisualPlayer::updateIcons(const PlayerIconData& icons) {
    auto* gm = GameManager::get();
    auto& settings = GlobedSettings::get();

    playerIcon->togglePlatformerMode(playLayer->m_level->isPlatformer());

    storedIcons = icons;
    if (settings.players.defaultDeathEffect) {
        // set the default one.. (aka do nothing ig?)
    } else {
        playerIcon->setDeathEffect(icons.deathEffect);
    }

    this->updatePlayerObjectIcons();
    this->updateIconType(playerIconType);
}

void ComplexVisualPlayer::updateData(const SpecificIconData& data, bool isDead, bool isPaused, bool isPracticing, bool isSpeaking) {
    playerIcon->setPosition(data.position);
    playerIcon->setRotation(data.rotation);

    // set the pos for status icons and name (ask rob not me)
    playerName->setPosition(data.position + CCPoint{0.f, 25.f});
    if (statusIcons) {
        statusIcons->setPosition(data.position + CCPoint{0.f, playerName->isVisible() ? 40.f : 25.f});
    }

    if (!isDead && playerIcon->getOpacity() == 0) {
        playerIcon->setOpacity(static_cast<unsigned char>(GlobedSettings::get().players.playerOpacity * 255.f));
    }

    PlayerIconType iconType = data.iconType;
    // in platformer, jetpack is serialized as ship so we make sure to show the right icon
    if (iconType == PlayerIconType::Ship && playLayer->m_level->isPlatformer()) {
        iconType = PlayerIconType::Jetpack;
    }

    // setFlipX doesnt work here for jetpack and stuff
    float mult = data.isMini ? 0.6f : 1.0f;
    playerIcon->setScaleX((data.isLookingLeft ? -1.0f : 1.0f) * mult);

    // swing is not flipped
    if (iconType == PlayerIconType::Swing) {
        playerIcon->setScaleY(mult);
    } else {
        playerIcon->setScaleY((data.isUpsideDown ? -1.0f : 1.0f) * mult);
    }

    bool switchedMode = iconType != playerIconType;

    bool turningOffSwing = (playerIconType == PlayerIconType::Swing && switchedMode);

    if (switchedMode) {
        this->updateIconType(iconType);
    }

    if (statusIcons) {
        statusIcons->updateStatus(isPaused, isPracticing, isSpeaking);
    }

    // animate robot and spider
    if (iconType == PlayerIconType::Robot || iconType == PlayerIconType::Spider) {
        if (wasGrounded != data.isGrounded || wasStationary != data.isStationary || wasFalling != data.isFalling || switchedMode) {
            wasGrounded = data.isGrounded;
            wasStationary = data.isStationary;
            wasFalling = data.isFalling;

            iconType == PlayerIconType::Robot ? this->updateRobotAnimation() : this->updateSpiderAnimation();
        }
    }

    // animate swing fire
    else if (iconType == PlayerIconType::Swing) {
        // if we just switched to swing, enable all fires
        if (switchedMode) {
            playerIcon->m_swingFireTop->setVisible(true);
            playerIcon->m_swingFireMiddle->setVisible(true);
            playerIcon->m_swingFireBottom->setVisible(true);

            playerIcon->m_swingFireMiddle->animateFireIn();
        }

        if (wasUpsideDown != data.isUpsideDown || switchedMode) {
            wasUpsideDown = data.isUpsideDown;
            this->animateSwingFire(!wasUpsideDown);
        }

        // now depending on the gravity, toggle either the bottom or top fire
    }

    // remove swing fire
    else if (turningOffSwing) {
        playerIcon->m_swingFireTop->setVisible(false);
        playerIcon->m_swingFireMiddle->setVisible(false);
        playerIcon->m_swingFireBottom->setVisible(false);

        playerIcon->m_swingFireTop->animateFireOut();
        playerIcon->m_swingFireMiddle->animateFireOut();
        playerIcon->m_swingFireBottom->animateFireOut();
    }

    this->setVisible(data.isVisible);
}

void ComplexVisualPlayer::updateName() {
    playerName->setString(parent->getAccountData().name.c_str());
    auto& sud = parent->getAccountData().specialUserData;
    sud.has_value() ? playerName->setColor(sud->nameColor) : playerName->setColor({255, 255, 255});
}

void ComplexVisualPlayer::updateIconType(PlayerIconType newType) {
    PlayerIconType oldType = playerIconType;
    playerIconType = newType;

    const auto& accountData = parent->getAccountData();
    const auto& icons = accountData.icons;

    this->toggleAllOff();

    if (newType != PlayerIconType::Cube) {
        this->callToggleWith(newType, true, false);
    }

    this->callUpdateWith(newType, this->getIconWithType(icons, newType));
}

void ComplexVisualPlayer::playDeathEffect() {
    // todo, doing simply ->playDeathEffect causes the hook to execute twice
    // if you figure out why then i love you
    playerIcon->PlayerObject::playDeathEffect();

    // TODO temp, we remove the small cube pieces because theyre buggy in my testing
    if (auto ein = getChildOfType<ExplodeItemNode>(this, 0)) {
        ein->removeFromParent();
    }
}

void ComplexVisualPlayer::playSpiderTeleport(const SpiderTeleportData& data) {
    playerIcon->m_unk65c = true;
    playerIcon->playSpiderDashEffect(data.from, data.to);
    playerIcon->stopActionByTag(SPIDER_TELEPORT_COLOR_ACTION);
    tpColorDelta = 0.f;

    this->spiderTeleportUpdateColor();
}

static inline ccColor3B lerpColor(ccColor3B from, ccColor3B to, float delta) {
    delta = std::clamp(delta, 0.f, 1.f);

    ccColor3B out;
    out.r = std::lerp(from.r, to.r, delta);
    out.g = std::lerp(from.g, to.g, delta);
    out.b = std::lerp(from.b, to.b, delta);

    return out;
}

void ComplexVisualPlayer::spiderTeleportUpdateColor() {
    constexpr float MAX_TIME = 0.4f;

    tpColorDelta += (1.f / 60.f);

    float delta = tpColorDelta / MAX_TIME;

    if (delta >= 1.f) {
        playerIcon->stopActionByTag(SPIDER_TELEPORT_COLOR_ACTION);
        playerIcon->setColor(storedMainColor);
        playerIcon->setSecondColor(storedSecondaryColor);
        return;
    }

    auto main = lerpColor(ccColor3B{255, 255, 255}, storedMainColor, delta);
    auto secondary = lerpColor(ccColor3B{255, 255, 255}, storedSecondaryColor, delta);

    playerIcon->setColor(main);
    playerIcon->setSecondColor(secondary);

    auto* seq = CCSequence::create(
        CCDelayTime::create(1.f / 60.f),
        CCCallFunc::create(this, callfunc_selector(ComplexVisualPlayer::spiderTeleportUpdateColor)),
        nullptr
    );
    seq->setTag(SPIDER_TELEPORT_COLOR_ACTION);

    this->runAction(seq);
}

void ComplexVisualPlayer::updateRobotAnimation() {
    if (wasGrounded && wasStationary) {
        // if on ground and not moving, play the idle animation
        playerIcon->m_robotSprite->tweenToAnimation("idle01", 0.1f);
        this->animateRobotFire(false);
    } else if (wasGrounded && !wasStationary) {
        // if on ground and moving, play the running animation
        playerIcon->m_robotSprite->tweenToAnimation("run", 0.1f);
        this->animateRobotFire(false);
    } else if (wasFalling) {
        // if in the air and falling, play falling animation
        playerIcon->m_robotSprite->tweenToAnimation("fall_loop", 0.1f);
        this->animateRobotFire(false);
    } else if (!wasFalling) {
        // if in the air and not falling, play jumping animation
        playerIcon->m_robotSprite->tweenToAnimation("jump_loop", 0.1f);
        this->animateRobotFire(true);
    }
}

void ComplexVisualPlayer::updateSpiderAnimation() {
    // this is practically the same as the robot animation

    if (!wasGrounded && wasFalling) {
        playerIcon->m_spiderSprite->tweenToAnimation("fall_loop", 0.1f);
    } else if (!wasGrounded && !wasFalling) {
        playerIcon->m_spiderSprite->tweenToAnimation("jump_loop", 0.1f);
    } else if (wasGrounded && wasStationary) {
        playerIcon->m_spiderSprite->tweenToAnimation("idle01", 0.1f);
    } else if (wasGrounded && !wasStationary) {
        playerIcon->m_spiderSprite->tweenToAnimation("run", 0.1f);
    }
}

void ComplexVisualPlayer::animateRobotFire(bool enable) {
    playerIcon->m_robotFire->stopActionByTag(ROBOT_FIRE_ACTION);

    CCSequence* seq;
    if (enable) {
        seq = CCSequence::create(
            CCDelayTime::create(0.15f),
            CCCallFunc::create(this, callfunc_selector(ComplexVisualPlayer::onAnimateRobotFireIn)),
            nullptr
        );

        playerIcon->m_robotFire->setVisible(true);
    } else {
        seq = CCSequence::create(
            CCDelayTime::create(0.1f),
            CCCallFunc::create(this, callfunc_selector(ComplexVisualPlayer::onAnimateRobotFireOut)),
            nullptr
        );

        playerIcon->m_robotFire->animateFireOut();
    }

    seq->setTag(ROBOT_FIRE_ACTION);
    playerIcon->m_robotFire->runAction(seq);
}

void ComplexVisualPlayer::onAnimateRobotFireIn() {
    playerIcon->m_robotFire->animateFireIn();
}

void ComplexVisualPlayer::animateSwingFire(bool goingDown) {
    if (goingDown) {
        playerIcon->m_swingFireTop->animateFireIn();
        playerIcon->m_swingFireBottom->animateFireOut();
    } else {
        playerIcon->m_swingFireTop->animateFireOut();
        playerIcon->m_swingFireBottom->animateFireIn();
    }
}

void ComplexVisualPlayer::onAnimateRobotFireOut() {
    playerIcon->m_robotFire->setVisible(false);
}

void ComplexVisualPlayer::updatePlayerObjectIcons() {
    auto* gm = GameManager::get();

    storedMainColor = gm->colorForIdx(storedIcons.color1);
    storedSecondaryColor = gm->colorForIdx(storedIcons.color2);

    playerIcon->setColor(storedMainColor);
    playerIcon->setSecondColor(storedSecondaryColor);

    if (storedIcons.glowColor != -1) {
        playerIcon->m_hasGlow = true;
        playerIcon->enableCustomGlowColor(gm->colorForIdx(storedIcons.glowColor));
    } else {
        playerIcon->m_hasGlow = false;
        playerIcon->disableCustomGlowColor();
    }

    playerIcon->updatePlayerShipFrame(storedIcons.ship);
    playerIcon->updatePlayerRollFrame(storedIcons.ball);
    playerIcon->updatePlayerBirdFrame(storedIcons.ufo);
    playerIcon->updatePlayerDartFrame(storedIcons.wave);
    playerIcon->updatePlayerRobotFrame(storedIcons.robot);
    playerIcon->updatePlayerSpiderFrame(storedIcons.spider);
    playerIcon->updatePlayerSwingFrame(storedIcons.swing);
    playerIcon->updatePlayerJetpackFrame(storedIcons.jetpack);
    playerIcon->updatePlayerFrame(storedIcons.cube);

    playerIcon->updateGlowColor();
    playerIcon->updatePlayerGlow();
}

void ComplexVisualPlayer::toggleAllOff() {
    playerIcon->toggleFlyMode(false, false);
    playerIcon->toggleRollMode(false, false);
    playerIcon->toggleBirdMode(false, false);
    playerIcon->toggleDartMode(false, false);
    playerIcon->toggleRobotMode(false, false);
    playerIcon->toggleSpiderMode(false, false);
    playerIcon->toggleSwingMode(false, false);
}

void ComplexVisualPlayer::callToggleWith(PlayerIconType type, bool arg1, bool arg2) {
    switch (type) {
    case PlayerIconType::Ship: playerIcon->toggleFlyMode(arg1, arg2); break;
    case PlayerIconType::Ball: playerIcon->toggleRollMode(arg1, arg2); break;
    case PlayerIconType::Ufo: playerIcon->toggleBirdMode(arg1, arg2); break;
    case PlayerIconType::Wave: playerIcon->toggleDartMode(arg1, arg2); break;
    case PlayerIconType::Robot: playerIcon->toggleRobotMode(arg1, arg2); break;
    case PlayerIconType::Spider: playerIcon->toggleSpiderMode(arg1, arg2); break;
    case PlayerIconType::Swing: playerIcon->toggleSwingMode(arg1, arg2); break;
    case PlayerIconType::Jetpack: playerIcon->toggleFlyMode(arg1, arg2); break;
    default: break;
    }
}

void ComplexVisualPlayer::callUpdateWith(PlayerIconType type, int icon) {
    switch (type) {
    case PlayerIconType::Cube: playerIcon->updatePlayerFrame(icon); break;
    case PlayerIconType::Ship: playerIcon->updatePlayerShipFrame(icon); break;
    case PlayerIconType::Ball: playerIcon->updatePlayerRollFrame(icon); break;
    case PlayerIconType::Ufo: playerIcon->updatePlayerBirdFrame(icon); break;
    case PlayerIconType::Wave: playerIcon->updatePlayerDartFrame(icon); break;
    case PlayerIconType::Robot: playerIcon->updatePlayerRobotFrame(icon); break;
    case PlayerIconType::Spider: playerIcon->updatePlayerSpiderFrame(icon); break;
    case PlayerIconType::Swing: playerIcon->updatePlayerSwingFrame(icon); break;
    case PlayerIconType::Jetpack: playerIcon->updatePlayerJetpackFrame(icon); break;
    case PlayerIconType::Unknown: break;
    }
}

cocos2d::CCPoint ComplexVisualPlayer::getPlayerPosition() {
    return playerIcon->getPosition();
}

ComplexVisualPlayer* ComplexVisualPlayer::create(RemotePlayer* parent, bool isSecond) {
    auto ret = new ComplexVisualPlayer;
    if (ret->init(parent, isSecond)) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}