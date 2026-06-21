#pragma once
#include <cstdint>
#include <cstring>
#include <atomic>
#include <string>
#include <cstdio>
#include <chrono>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace vn {
namespace ipc {

static constexpr uint32_t SHM_MAGIC       = 0x564E5053u; // "VNPS"
static constexpr uint32_t SHM_VERSION     = 2u;          // bumped for telemetry
static constexpr uint32_t SHM_SLOT_COUNT  = 4u;
static constexpr uint32_t SHM_MAX_AGENTS  = 1024u;

#pragma pack(push, 4)
struct AgentSnapshot {
    uint32_t agent_id;
    float     pos_x, pos_y, pos_z;
    float     pillars[16];
};

struct TelemetrySlot {
    uint64_t c_write_count;       // total writes performed by C++ side
    uint64_t c_write_last_ns;     // last write latency (ns)
    uint64_t c_write_max_ns;      // max write latency seen (ns)
    uint64_t c_write_last_tsc;    // host timestamp (ns since epoch) of last write
};

struct RingSlot {
    uint64_t seq;         // 0 = empty; incremented on each write
    uint64_t tick;
    uint32_t agent_count;
    uint32_t pad;
    AgentSnapshot agents[SHM_MAX_AGENTS];
};

struct SHMHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t slot_count;
    uint32_t max_agents;
    uint64_t total_size;
    uint64_t write_seq;
    TelemetrySlot telemetry;
};
#pragma pack(pop)

static_assert(sizeof(TelemetrySlot) == 32, "TelemetrySlot should be 32 bytes");
static_assert(sizeof(SHMHeader) == 64, "SHMHeader should be 64 bytes (telemetry replaces pad[4])");

// Layout: [SHMHeader][RingSlot 0][RingSlot 1]...
static constexpr uint64_t SHM_HEADER_SIZE  = sizeof(SHMHeader);
static constexpr uint64_t SHM_SLOT_SIZE    = sizeof(RingSlot);
static constexpr uint64_t SHM_TOTAL_SIZE   = SHM_HEADER_SIZE + SHM_SLOT_COUNT * SHM_SLOT_SIZE;

class SharedMemoryRingBuffer {
public:
    SharedMemoryRingBuffer() = default;
    ~SharedMemoryRingBuffer() { shutdown(); }

    SharedMemoryRingBuffer(const SharedMemoryRingBuffer&) = delete;
    SharedMemoryRingBuffer& operator=(const SharedMemoryRingBuffer&) = delete;

    bool init(const char* shm_name = "VanNueman_PSV_Ring") {
        if (mapped_) return false;
        name_ = shm_name;

#ifdef _WIN32
        hMap_ = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0,
            static_cast<DWORD>(SHM_TOTAL_SIZE),
            shm_name
        );
        if (!hMap_) return false;

        mapped_ = MapViewOfFile(hMap_, FILE_MAP_ALL_ACCESS, 0, 0, SHM_TOTAL_SIZE);
        if (!mapped_) {
            CloseHandle(hMap_);
            hMap_ = nullptr;
            return false;
        }
#else
        shm_fd_ = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
        if (shm_fd_ < 0) return false;

        if (ftruncate(shm_fd_, static_cast<off_t>(SHM_TOTAL_SIZE)) < 0) {
            close(shm_fd_);
            shm_fd_ = -1;
            return false;
        }

        mapped_ = mmap(nullptr, SHM_TOTAL_SIZE, PROT_READ | PROT_WRITE,
                       MAP_SHARED, shm_fd_, 0);
        if (mapped_ == MAP_FAILED) {
            close(shm_fd_);
            shm_fd_ = -1;
            mapped_ = nullptr;
            return false;
        }
#endif

        header_ = static_cast<SHMHeader*>(mapped_);
        if (header_->magic != SHM_MAGIC || header_->version != SHM_VERSION) {
            std::memset(mapped_, 0, SHM_TOTAL_SIZE);
            header_->magic       = SHM_MAGIC;
            header_->version     = SHM_VERSION;
            header_->slot_count  = SHM_SLOT_COUNT;
            header_->max_agents  = SHM_MAX_AGENTS;
            header_->total_size  = SHM_TOTAL_SIZE;
            header_->write_seq   = 0;
            // telemetry is zeroed by memset
        }

        return true;
    }

    template <typename Fn>
    uint32_t write_snapshot(uint64_t tick, uint32_t total_count, Fn&& cb) {
        if (!header_) return 0;

        auto t0 = std::chrono::high_resolution_clock::now();

        uint64_t slot_idx = header_->write_seq % SHM_SLOT_COUNT;
        RingSlot* slot    = slot_ptr(slot_idx);

        uint32_t written = 0;
        for (uint32_t i = 0; i < total_count && written < SHM_MAX_AGENTS; i++) {
            AgentSnapshot asnap;
            if (!cb(i, asnap)) continue;
            slot->agents[written++] = asnap;
        }

        std::atomic_thread_fence(std::memory_order_release);
        slot->agent_count = written;
        slot->tick        = tick;
        slot->seq         = header_->write_seq + 1;

        std::atomic_thread_fence(std::memory_order_release);
        header_->write_seq++;

        auto t1 = std::chrono::high_resolution_clock::now();
        uint64_t elapsed_ns = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

        TelemetrySlot& tm = header_->telemetry;
        tm.c_write_count++;
        tm.c_write_last_ns = elapsed_ns;
        if (elapsed_ns > tm.c_write_max_ns) tm.c_write_max_ns = elapsed_ns;
        tm.c_write_last_tsc = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
            t1.time_since_epoch()).count();

        return written;
    }

    void shutdown() {
        if (!mapped_) return;

#ifdef _WIN32
        UnmapViewOfFile(mapped_);
        if (hMap_) CloseHandle(hMap_);
        hMap_ = nullptr;
#else
        munmap(mapped_, SHM_TOTAL_SIZE);
        if (shm_fd_ >= 0) {
            close(shm_fd_);
            shm_unlink(name_.c_str());
        }
        shm_fd_ = -1;
#endif
        mapped_ = nullptr;
        header_ = nullptr;
    }

    bool is_initialized() const { return mapped_ != nullptr; }

private:
    RingSlot* slot_ptr(uint64_t index) const {
        auto* base = static_cast<uint8_t*>(mapped_);
        return reinterpret_cast<RingSlot*>(base + SHM_HEADER_SIZE + index * SHM_SLOT_SIZE);
    }

    std::string name_;
#ifdef _WIN32
    HANDLE  hMap_   = nullptr;
#else
    int     shm_fd_ = -1;
#endif
    void*   mapped_ = nullptr;
    SHMHeader* header_ = nullptr;

public:
    const SHMHeader* get_header() const { return header_; }
};

} // namespace ipc
} // namespace vn
