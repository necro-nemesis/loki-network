#ifndef LLARP_RC_LOOKUP_HANDLER_HPP
#define LLARP_RC_LOOKUP_HANDLER_HPP

#include <chrono>
#include <router/i_rc_lookup_handler.hpp>

#include <util/thread/threading.hpp>

#include <unordered_map>
#include <set>
#include <unordered_set>
#include <list>

struct llarp_dht_context;

namespace llarp
{
  class NodeDB;
  class EventLoop;

  namespace service
  {
    struct Context;

  }  // namespace service

  struct ILinkManager;

  struct RCLookupHandler final : public I_RCLookupHandler
  {
   public:
    using Work_t = std::function<void(void)>;
    using WorkerFunc_t = std::function<void(Work_t)>;
    using CallbacksQueue = std::list<RCRequestCallback>;

    ~RCLookupHandler() override = default;

    void
    AddValidRouter(const RouterID& router) override EXCLUDES(_mutex);

    void
    RemoveValidRouter(const RouterID& router) override EXCLUDES(_mutex);

    void
    SetRouterWhitelist(const std::vector<RouterID>& routers) override EXCLUDES(_mutex);

    bool
    HaveReceivedWhitelist();

    void
    GetRC(const RouterID& router, RCRequestCallback callback, bool forceLookup = false) override
        EXCLUDES(_mutex);

    bool
    RemoteIsAllowed(const RouterID& remote) const override EXCLUDES(_mutex);

    bool
    CheckRC(const RouterContact& rc) const override;

    bool
    GetRandomWhitelistRouter(RouterID& router) const override EXCLUDES(_mutex);

    bool
    CheckRenegotiateValid(RouterContact newrc, RouterContact oldrc) override;

    void
    PeriodicUpdate(llarp_time_t now) override;

    void
    ExploreNetwork() override;

    size_t
    NumberOfStrictConnectRouters() const override;

    void
    Init(
        llarp_dht_context* dht,
        std::shared_ptr<NodeDB> nodedb,
        std::shared_ptr<EventLoop> loop,
        WorkerFunc_t dowork,
        ILinkManager* linkManager,
        service::Context* hiddenServiceContext,
        const std::unordered_set<RouterID>& strictConnectPubkeys,
        const std::set<RouterContact>& bootstrapRCList,
        bool useWhitelist_arg,
        bool isServiceNode_arg);

   private:
    void
    HandleDHTLookupResult(RouterID remote, const std::vector<RouterContact>& results);

    bool
    HavePendingLookup(RouterID remote) const EXCLUDES(_mutex);

    bool
    RemoteInBootstrap(const RouterID& remote) const;

    void
    FinalizeRequest(const RouterID& router, const RouterContact* const rc, RCRequestResult result)
        EXCLUDES(_mutex);

    mutable util::Mutex _mutex;  // protects pendingCallbacks, whitelistRouters

    llarp_dht_context* _dht = nullptr;
    std::shared_ptr<NodeDB> _nodedb;
    std::shared_ptr<EventLoop> _loop;
    WorkerFunc_t _work = nullptr;
    service::Context* _hiddenServiceContext = nullptr;
    ILinkManager* _linkManager = nullptr;

    /// explicit whitelist of routers we will connect to directly (not for
    /// service nodes)
    std::unordered_set<RouterID> _strictConnectPubkeys;

    std::set<RouterContact> _bootstrapRCList;
    std::unordered_set<RouterID> _bootstrapRouterIDList;

    std::unordered_map<RouterID, CallbacksQueue, RouterID::Hash> pendingCallbacks
        GUARDED_BY(_mutex);

    bool useWhitelist = false;
    bool isServiceNode = false;

    std::unordered_set<RouterID> whitelistRouters GUARDED_BY(_mutex);

    using TimePoint = std::chrono::steady_clock::time_point;
    std::unordered_map<RouterID, TimePoint, RouterID::Hash> _routerLookupTimes;
  };

}  // namespace llarp

#endif  // LLARP_RC_LOOKUP_HANDLER_HPP
