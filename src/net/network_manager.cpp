#include "network_manager.hpp"

#include <data/packets/all.hpp>
#include <managers/error_queues.hpp>
#include <managers/account.hpp>
#include <managers/profile_cache.hpp>
#include <util/net.hpp>
#include <util/debug.hpp>

using namespace geode::prelude;
using namespace util::data;

NetworkManager::NetworkManager() {
    util::net::initialize();

    if (!gameSocket.create()) util::net::throwLastError();

    // add builtin listeners for connection related packets

    addBuiltinListener<CryptoHandshakeResponsePacket>([this](auto packet) {
        gameSocket.box->setPeerKey(packet->data.key.data());
        _handshaken = true;
        // and lets try to login!
        auto& am = GlobedAccountManager::get();
        std::string authtoken;

        if (!_connectingStandalone) {
            authtoken = *am.authToken.lock();
        }

        auto& pcm = ProfileCacheManager::get();
        pcm.setOwnDataAuto();
        pcm.pendingChanges = false;

        auto gddata = am.gdData.lock();
        auto pkt = LoginPacket::create(gddata->accountId, gddata->accountName, authtoken, pcm.getOwnData());
        this->send(pkt);
    });

    addBuiltinListener<KeepaliveResponsePacket>([](auto packet) {
        GameServerManager::get().finishKeepalive(packet->playerCount);
    });

    addBuiltinListener<ServerDisconnectPacket>([this](auto packet) {
        ErrorQueues::get().error(fmt::format("You have been disconnected from the active server.\n\nReason: <cy>{}</c>", packet->message));
        this->disconnect(true);
    });

    addBuiltinListener<LoggedInPacket>([this](auto packet) {
        log::info("Successfully logged into the server!");
        connectedTps = packet->tps;
        _loggedin = true;
    });

    addBuiltinListener<LoginFailedPacket>([this](auto packet) {
        ErrorQueues::get().error(fmt::format("<cr>Authentication failed!</c> Please try to connect again, if it still doesn't work then reset your authtoken in settings.\n\nReason: <cy>{}</c>", packet->message));
        GlobedAccountManager::get().authToken.lock()->clear();
        this->disconnect(true);
    });

    addBuiltinListener<ServerNoticePacket>([](auto packet) {
        ErrorQueues::get().notice(packet->message);
    });

    addBuiltinListener<ProtocolMismatchPacket>([this](auto packet) {
        std::string message;
        if (packet->serverProtocol < PROTOCOL_VERSION) {
            message = fmt::format(
                "Outdated server! This server uses protocol <cy>v{}</c>, while your client is using protocol <cy>v{}</c>. Downgrade the mod to an older version or ask the server owner to update their server.",
                packet->serverProtocol, PROTOCOL_VERSION
            );
        } else {
            message = fmt::format(
                "Outdated client! Please update the mod to the latest version in order to connect. The server is using protocol <cy>v{}</c>, while this version of the mod only supports protocol <cy>v{}</c>.",
                packet->serverProtocol, PROTOCOL_VERSION
            );
        }

        ErrorQueues::get().error(message);
        this->disconnect(true);
    });

    addBuiltinListener<AdminAuthSuccessPacket>([this](auto packet) {
        _adminAuthorized = true;
        ErrorQueues::get().success("Successfully authorized");
    });

    // boot up the threads

    threadMain.setLoopFunction(&NetworkManager::threadMainFunc);
    threadMain.setName("Network (out) Thread");
    threadMain.start(this);

    threadRecv.setLoopFunction(&NetworkManager::threadRecvFunc);
    threadRecv.setName("Network (in) Thread");
    threadRecv.start(this);
}

NetworkManager::~NetworkManager() {
    log::debug("cleaning up..");

    // clear listeners
    this->removeAllListeners();
    builtinListeners.lock()->clear();

    threadMain.stopAndWait();
    threadRecv.stopAndWait();

    if (this->connected()) {
        log::debug("disconnecting from the server..");
        try {
            this->disconnect(false, true);
        } catch (const std::exception& e) {
            log::warn("error trying to disconnect: {}", e.what());
        }
    }

    util::net::cleanup();

    log::debug("Goodbye!");
}

Result<> NetworkManager::connect(const std::string_view addr, unsigned short port, bool standalone) {
    if (this->connected() && !this->handshaken()) {
        return Err("already trying to connect, please wait");
    }

    if (this->connected()) {
        this->disconnect(false);
    }

    _connectingStandalone = standalone;

    lastReceivedPacket = util::time::now();

    if (!standalone) {
        GLOBED_REQUIRE_SAFE(!GlobedAccountManager::get().authToken.lock()->empty(), "attempting to connect with no authtoken set in account manager")
    }

    GLOBED_UNWRAP(gameSocket.connect(addr, port));
    gameSocket.createBox();

    auto packet = CryptoHandshakeStartPacket::create(PROTOCOL_VERSION, CryptoPublicKey(gameSocket.box->extractPublicKey()));
    this->send(packet);

    return Ok();
}

Result<> NetworkManager::connectWithView(const GameServer& gsview) {
    auto result = this->connect(gsview.address.ip, gsview.address.port);
    if (result.isOk()) {
        GameServerManager::get().setActive(gsview.id);
        return Ok();
    } else {
        this->disconnect(true);
        return Err(result.unwrapErr());
    }
}

Result<> NetworkManager::connectStandalone() {
    auto _server = GameServerManager::get().getServer(GameServerManager::STANDALONE_ID);
    if (!_server.has_value()) {
        return Err(fmt::format("failed to find server by standalone ID"));
    }

    auto server = _server.value();

    auto result = this->connect(server.address.ip, server.address.port, true);
    if (result.isOk()) {
        GameServerManager::get().setActive(GameServerManager::STANDALONE_ID);
        return Ok();
    } else {
        this->disconnect(true);
        return Err(result.unwrapErr());
    }
}

void NetworkManager::disconnect(bool quiet, bool noclear) {
    if (!this->connected()) {
        return;
    }

    if (!quiet) {
        // send it directly instead of pushing to the queue
        gameSocket.sendPacket(DisconnectPacket::create());
    }

    _handshaken = false;
    _loggedin = false;
    _connectingStandalone = false;
    _adminAuthorized = false;

    gameSocket.disconnect();
    gameSocket.cleanupBox();

    // GameServerManager could have been destructed before NetworkManager, so this could be UB. Additionally will break autoconnect.
    if (!noclear) {
        GameServerManager::get().clearActive();
    }
}

void NetworkManager::send(std::shared_ptr<Packet> packet) {
    GLOBED_REQUIRE(this->connected(), "tried to send a packet while disconnected")
    packetQueue.push(std::move(packet));
}

void NetworkManager::addListener(packetid_t id, PacketCallback&& callback) {
    (*listeners.lock())[id] = std::move(callback);
}

void NetworkManager::removeListener(packetid_t id) {
    listeners.lock()->erase(id);
}

void NetworkManager::removeAllListeners() {
    listeners.lock()->clear();
}

// tasks

void NetworkManager::taskPingServers() {
    taskQueue.push(NetworkThreadTask::PingServers);
}

// threads

void NetworkManager::threadMainFunc() {
    if (_suspended) {
        std::this_thread::sleep_for(util::time::millis(250));
        return;
    }

    this->maybeSendKeepalive();

    if (!packetQueue.waitForMessages(util::time::millis(250))) {
        // check for tasks
        if (taskQueue.empty()) return;

        for (const auto& task : taskQueue.popAll()) {
            if (task == NetworkThreadTask::PingServers) {
                auto& sm = GameServerManager::get();
                auto activeServer = sm.getActiveId();

                for (auto& [serverId, server] : sm.getAllServers()) {
                    if (serverId == activeServer) continue;

                    try {
                        auto pingId = sm.startPing(serverId);
                        gameSocket.sendPacketTo(PingPacket::create(pingId), server.address.ip, server.address.port);
                    } catch (const std::exception& e) {
                        ErrorQueues::get().warn(e.what());
                    }
                }
            }
        }
    }

    auto messages = packetQueue.popAll();

    for (auto packet : messages) {
        try {
            gameSocket.sendPacket(packet);
        } catch (const std::exception& e) {
            ErrorQueues::get().error(e.what());
        }
    }
}

void NetworkManager::threadRecvFunc() {
    if (_suspended) {
        std::this_thread::sleep_for(util::time::millis(250));
        return;
    }

    auto result = gameSocket.poll(1000);
    if (result.isErr()) {
        ErrorQueues::get().debugWarn(fmt::format("poll failed: {}", result.unwrapErr()));
        return;
    }

    if (!result.unwrap()) {
        this->maybeDisconnectIfDead();
        return;
    }

    GameSocket::IncomingPacket packet;

    try {
        packet = gameSocket.recvPacket();
    } catch (const std::exception& e) {
        ErrorQueues::get().debugWarn(fmt::format("failed to receive a packet: {}", e.what()));
        return;
    }

    packetid_t packetId = packet.packet->getPacketId();

    if (packetId == PingResponsePacket::PACKET_ID) {
        this->handlePingResponse(packet.packet);
        return;
    }

    // if it's not a ping packet, and it's NOT from the currently connected server, we reject it
    if (!packet.fromServer) {
        return;
    }

    lastReceivedPacket = util::time::now();

    auto builtin = builtinListeners.lock();
    if (builtin->contains(packetId)) {
        (*builtin)[packetId](packet.packet);
        return;
    }

    // this is scary
    Loader::get()->queueInMainThread([this, packetId, packet]() {
        auto listeners_ = this->listeners.lock();
        if (!listeners_->contains(packetId)) {
            auto suppressed_ = suppressed.lock();

            if (suppressed_->contains(packetId) && util::time::systemNow() > suppressed_->at(packetId)) {
                suppressed_->erase(packetId);
            }

            if (!suppressed_->contains(packetId)) {
                ErrorQueues::get().debugWarn(fmt::format("Unhandled packet: {}", packetId));
            }
        } else {
            // xd
            (*listeners_)[packetId](packet.packet);
        }
    });
}

void NetworkManager::handlePingResponse(std::shared_ptr<Packet> packet) {
    if (PingResponsePacket* pingr = dynamic_cast<PingResponsePacket*>(packet.get())) {
        GameServerManager::get().finishPing(pingr->id, pingr->playerCount);
    }
}

void NetworkManager::maybeSendKeepalive() {
    if (_loggedin) {
        auto now = util::time::now();
        if ((now - lastKeepalive) > KEEPALIVE_INTERVAL) {
            lastKeepalive = now;
            this->send(KeepalivePacket::create());
            GameServerManager::get().startKeepalive();
        }
    }
}

// Disconnects from the server if there has been no response for a while
void NetworkManager::maybeDisconnectIfDead() {
    if (!this->connected()) return;

    auto elapsed = util::time::now() - lastReceivedPacket;

    // if we haven't had a handshake response in 5 seconds, assume the server is dead
    if (!this->handshaken() && elapsed > util::time::seconds(5)) {
        ErrorQueues::get().error("Failed to connect to the server. No response was received after 5 seconds.");
        this->disconnect(true);
    } else if (elapsed > DISCONNECT_AFTER) {
        ErrorQueues::get().error("The server you were connected to is not responding to any requests. <cy>You have been disconnected.</c>");
        try {
            this->disconnect();
        } catch (const std::exception& e) {
            log::warn("failed to disconnect from a dead server: {}", e.what());
        }
    }
}

void NetworkManager::addBuiltinListener(packetid_t id, PacketCallback&& callback) {
    (*builtinListeners.lock())[id] = std::move(callback);
}

bool NetworkManager::connected() {
    return gameSocket.connected;
}

bool NetworkManager::handshaken() {
    return _handshaken;
}

bool NetworkManager::established() {
    return _loggedin;
}

bool NetworkManager::isAuthorizedAdmin() {
    return _adminAuthorized;
}

bool NetworkManager::standalone() {
    return _connectingStandalone;
}

void NetworkManager::suspend() {
    _suspended = true;
}

void NetworkManager::resume() {
    _suspended = false;
}