#pragma once
#include <defs.hpp>

class FriendListManager : public SingletonBase<FriendListManager> {
protected:
    friend class SingletonBase;
    friend class DummyNode;

    class DummyNode : public cocos2d::CCNode, public UserListDelegate {
    public:
        static DummyNode* create();
        void cleanup();
        void getUserListFinished(cocos2d::CCArray* p0, UserListType p1);
        void getUserListFailed(UserListType p0, GJErrorCode p1);
        void userListChanged(cocos2d::CCArray* p0, UserListType p1);
        void forceReloadList(UserListType p0);
    };

public:
    // makes an async request to gd servers and fetches the current friendlist
    void load();
    bool isLoaded();

    // reset the friend list and ensure `isLoaded()` will return `false` until reloaded again.
    void invalidate();

    bool isFriend(int playerId);

private:
    void insertPlayers(cocos2d::CCArray* players);

    Ref<DummyNode> dummyNode;
    std::set<int> friends;
    bool loaded = false;
};