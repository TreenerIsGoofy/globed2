#pragma once
#include <defs.hpp>

#define GDEFAULTKEY(name) _DefaultFor##name
#define GDEFAULT(name, type, val) static constexpr type GDEFAULTKEY(name) = val;
#define GSETTING(ty, name, default_) \
    GDEFAULT(name, ty, default_); \
    ty name = GDEFAULTKEY(name); \

// This class should only be accessed from the main thread.
class GlobedSettings : public SingletonBase<GlobedSettings> {
protected:
    friend class SingletonBase;
    GlobedSettings();

public:
    struct Globed {
        GSETTING(bool, autoconnect, true);
        GSETTING(int, tpsCap, 0);
    };

    struct Overlay {
        GSETTING(bool, enabled, true);
        GSETTING(float, opacity, 0.3f);
        GSETTING(bool, hideConditionally, false);
        GSETTING(int, position, 3); // 0-3 topleft, topright, bottomleft, bottomright
    };

    struct Communication {
        GSETTING(bool, voiceEnabled, true);
        GSETTING(bool, voiceProximity, true);
        GSETTING(float, voiceVolume, 1.0f);
        GSETTING(bool, onlyFriends, false);
        GSETTING(bool, lowerAudioLatency, false);
        GSETTING(int, audioDevice, 0);
    };

    struct LevelUI {
        GSETTING(bool, progressIndicators, true);
        GSETTING(bool, progressPointers, true); // unused
    };

    struct Players {
        GSETTING(float, playerOpacity, 1.0f);
        GSETTING(bool, showNames, true);
        GSETTING(bool, dualName, true);
        GSETTING(float, nameOpacity, 1.0f);
        GSETTING(bool, statusIcons, true);
        GSETTING(bool, deathEffects, true);
        GSETTING(bool, defaultDeathEffect, false);
    };

    struct Advanced {};

    struct Flags {
        bool seenSignupNotice = false;
    };

    Globed globed;
    Overlay overlay;
    Communication communication;
    LevelUI levelUi;
    Players players;
    Advanced advanced;
    Flags flags;

    void save();
    void reload();
    void resetToDefaults();
    void clear(const std::string_view key);

private:
    void hardReset();

    template <typename T>
    void store(const std::string_view key, const T& val) {
        Mod::get()->setSavedValue(key, val);
    }

    bool has(const std::string_view key) {
        return Mod::get()->hasSavedValue(key);
    }

    template <typename T>
    T load(const std::string_view key) {
        return Mod::get()->getSavedValue<T>(key);
    }

    // If setting is present, loads into `into`. Otherwise does nothing.
    template <typename T>
    void loadOptionalInto(const std::string_view key, T& into) {
        if (this->has(key)) {
            into = this->load<T>(key);
        }
    }

    template <typename T>
    std::optional<T> loadOptional(const std::string_view key) {
        return this->has(key) ? this->load<T>(key) : std::nullopt;
    }

    template <typename T>
    T loadOrDefault(const std::string_view key, const T defaultval) {
        return this->has(key) ? this->load<T>(key) : defaultval;
    }
};

#undef GDEFAULTKEY
#undef GDEFAULT
#undef GSETTING