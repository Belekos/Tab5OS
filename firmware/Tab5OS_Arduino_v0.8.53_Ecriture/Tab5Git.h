#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Tab5Resources { class Reservation; }

// All functions below run on a worker. libgit2 sessions are serialized, bounded
// in PSRAM, and released on return. No Git object is retained by the UI.
namespace Tab5Git {
using Output = std::function<void(const char*,size_t)>;
using Cancel = std::function<bool()>;
using Translate = std::function<std::string(const char*)>;
enum Status : uint32_t {
  IndexNew=1u, IndexModified=2u, IndexDeleted=4u, IndexRenamed=8u, IndexTypeChange=16u,
  WorkNew=128u, WorkModified=256u, WorkDeleted=512u, WorkTypeChange=1024u, WorkRenamed=2048u,
  Ignored=16384u, Conflicted=32768u
};
struct Observer {
  // Called synchronously on the worker; copy strings before returning.
  std::function<void(const char* path,uint32_t flags)> status;
  std::function<void(const char* branch,bool unborn,bool detached)> repository;
  std::function<void(const char* logicalRoot)> project;
  std::function<void(const char* name,const char* url)> remote;
  std::function<void(const char* key,const char* value)> config;
  std::function<void()> notRepository;
  std::function<void()> worktreeChanged;
  // Last fetched reference, never a claim about an uncontacted remote.
  std::function<void(const char* remote,const char* branch,size_t ahead,size_t behind,bool known)> upstream;
};
constexpr size_t MemoryLimit=8u*1024u*1024u;
constexpr size_t WorkerStackBytes=40u*1024u;
bool busy();
// Tests may inject a resolver; production resolves the unlocked encrypted vault.
// Only names are stored in .git/config. Callback buffers never escape the call.
using SecretResolver=std::function<bool(const char*,char*,size_t)>;
void secretResolver(SecretResolver resolver);
// argv includes "git". root is the physical volume prefix ("/sd" on Tab5),
// cwd is logical ("/Projects", "/USB/Projects"). -C accepts logical paths.
// Returns -1 for another command, 0 success, 2 bad syntax, 130 interrupted,
// 1 operational failure. Diagnostics and stdout are separate for shell pipes.
int run(const std::string& root,const std::string& cwd,
        const std::vector<std::string>& argv,Output output,Cancel cancel={},
        Translate translate={},Output diagnostics={},Observer observer={},
        Tab5Resources::Reservation* startupAdmission=nullptr);
// startupAdmission is an optional firmware working-set reservation acquired
// before the worker stack is created. run() consumes it on every exit path;
// the caller must not access it until run() returns.
// Terminal adapter: waits for a separate admitted worker, so nested shell
// scripts and substitutions do not consume Git's stack. Never call on the UI.
int runIsolated(const std::string& root,const std::string& cwd,
        const std::vector<std::string>& argv,Output output,Cancel cancel={},
        Translate translate={},Output diagnostics={});
// Measured allocator counters for the last completed session (not SRAM).
struct MemoryStats {size_t current=0,peak=0,limit=MemoryLimit,reclaimedBlocks=0,reclaimedBytes=0;};
MemoryStats memoryStats();
}
