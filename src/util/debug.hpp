#pragma once
#include <defs.hpp>

#include <unordered_map>
#ifdef GEODE_IS_ANDROID
# include <cxxabi.h>
#endif

#include <data/packets/packet.hpp>
#include <util/collections.hpp>
#include <util/time.hpp>
#include <util/sync.hpp>
#include <util/misc.hpp>

namespace util::debug {
    class Benchmarker : public SingletonBase<Benchmarker> {
    public:
        void start(const std::string_view id);
        void endAndLog(const std::string_view id);
        time::micros end(const std::string_view id);
        time::micros run(std::function<void()>&& func);
        void runAndLog(std::function<void()>&& func, const std::string_view identifier);

    private:
        std::unordered_map<std::string, time::time_point> _entries;
    };

    class DataWatcher : public SingletonBase<DataWatcher> {
    public:
        struct WatcherEntry {
            uintptr_t address;
            size_t size;
            data::bytevector lastData;
        };

        void start(const std::string_view id, uintptr_t address, size_t size) {
            auto idstr = std::string(id);
            _entries.emplace(std::string(idstr), WatcherEntry {
                .address = address,
                .size = size,
                .lastData = data::bytevector(size)
            });

            this->updateLastData(_entries.at(idstr));
        }

        void start(const std::string_view id, void* address, size_t size) {
            this->start(id, (uintptr_t)address, size);
        }

        // returns the indexes of bytes that were modified since last read
        std::vector<size_t> updateLastData(WatcherEntry& entry);

        void updateAll();

    private:
        std::unordered_map<std::string, WatcherEntry> _entries;
    };

    struct PacketLog {
        packetid_t id;
        bool encrypted;
        bool outgoing;
        size_t bytes;
    };

    struct PacketLogSummary {
        size_t total;

        size_t totalIn;
        size_t totalOut;

        size_t totalCleartext;
        size_t totalEncrypted;

        uint64_t totalBytes;
        uint64_t totalBytesIn;
        uint64_t totalBytesOut;

        std::unordered_map<packetid_t, size_t> packetCounts;

        float bytesPerPacket;
        float encryptedRatio;

        void print();
    };

    class PacketLogger : public SingletonBase<PacketLogger> {
    public:
        void record(packetid_t id, bool encrypted, bool outgoing, size_t bytes) {
# ifdef GLOBED_DEBUG_PACKETS
#  ifdef GLOBED_DEBUG_PACKETS_PRINT
            log::debug("{} packet {}, encrypted: {}, bytes: {}", outgoing ? "Sending" : "Receiving", id, encrypted ? "true" : "false", bytes);
#  endif // GLOBED_DEBUG_PACKETS_PRINT
            queue.push(PacketLog {
                .id = id,
                .encrypted = encrypted,
                .outgoing = outgoing,
                .bytes = bytes
            });
# endif // GLOBED_DEBUG_PACKETS
        }

        PacketLogSummary getSummary();
    private:
        collections::CappedQueue<PacketLog, 25000> queue;
    };

    std::string hexDumpAddress(uintptr_t addr, size_t bytes);
    std::string hexDumpAddress(void* ptr, size_t bytes);

#if GLOBED_CAN_USE_SOURCE_LOCATION
    std::string sourceLocation(const std::source_location loc = GLOBED_SOURCE);
    // crash the program immediately, print the location of the caller
    [[noreturn]] void suicide(const std::source_location loc = GLOBED_SOURCE);
#else
    std::string sourceLocation();
    // crash the program immediately
    [[noreturn]] void suicide();
#endif

    // like log::debug but with precise timestamps.
    void timedLog(const std::string_view message);

    // send a log to a different thread
    template <typename... Args>
    void fastLog(fmt::format_string<Args...> fmtString, Args&&... args) {
        static misc::OnceCell<sync::SmartMessageQueue<std::string>> _mq;
        sync::SmartMessageQueue<std::string>& mq = _mq.getOrInit([] {
            return sync::SmartMessageQueue<std::string>();
        });

        util::misc::callOnceSync("log-thread-init", [&mq] {
            sync::SmartThread<> thr;
            thr.setName("log thread");
            thr.setLoopFunction([&mq] {
                auto messages = mq.popAll();
                for (const auto& message : messages) {
                    log::debug("{}", message);
                }
            });
            thr.start();
            thr.detach();
        });

        mq.push(fmt::format(fmtString, std::forward<Args>(args)...));
    }

    struct ProcMapEntry {
        ptrdiff_t size;
        bool readable;
    };

    Result<std::string> getTypename(void* address);
    Result<std::string> getTypenameFromVtable(void* address);

    void dumpStruct(void* address, size_t size);

    std::optional<ptrdiff_t> searchMember(const void* structptr, const uint8_t* bits, size_t length, size_t alignment, size_t maxSize);

    template <typename T>
    std::optional<ptrdiff_t> searchMember(const void* structptr, const T& value, size_t maxSize) {
        return searchMember(structptr, reinterpret_cast<const uint8_t*>(&value), sizeof(T), alignof(T), maxSize);
    }
}
