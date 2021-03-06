#include "link_manager.hpp"

#include <llarp/router/i_outbound_session_maker.hpp>
#include <llarp/crypto/crypto.hpp>

#include <algorithm>
#include <set>

namespace llarp
{
  LinkLayer_ptr
  LinkManager::GetCompatibleLink(const RouterContact& rc) const
  {
    if (stopping)
      return nullptr;

    for (auto& link : outboundLinks)
    {
      // TODO: may want to add some memory of session failures for a given
      //      router on a given link and not return that link here for a
      //      duration
      if (!link->IsCompatable(rc))
        continue;

      return link;
    }

    return nullptr;
  }

  IOutboundSessionMaker*
  LinkManager::GetSessionMaker() const
  {
    return _sessionMaker;
  }

  bool
  LinkManager::SendTo(
      const RouterID& remote, const llarp_buffer_t& buf, ILinkSession::CompletionHandler completed)
  {
    if (stopping)
      return false;

    auto link = GetLinkWithSessionTo(remote);
    if (link == nullptr)
    {
      if (completed)
      {
        completed(ILinkSession::DeliveryStatus::eDeliveryDropped);
      }
      return false;
    }

    return link->SendTo(remote, buf, completed);
  }

  bool
  LinkManager::HasSessionTo(const RouterID& remote) const
  {
    return GetLinkWithSessionTo(remote) != nullptr;
  }

  std::optional<bool>
  LinkManager::SessionIsClient(RouterID remote) const
  {
    for (const auto& link : inboundLinks)
    {
      const auto session = link->FindSessionByPubkey(remote);
      if (session)
        return not session->IsRelay();
    }
    for (const auto& link : outboundLinks)
    {
      if (link->HasSessionTo(remote))
        return false;
    }
    return std::nullopt;
  }

  void
  LinkManager::DeregisterPeer(RouterID remote)
  {
    m_PersistingSessions.erase(remote);
    for (const auto& link : inboundLinks)
    {
      link->CloseSessionTo(remote);
    }
    for (const auto& link : outboundLinks)
    {
      link->CloseSessionTo(remote);
    }
    LogInfo(remote, " has been de-registered");
  }

  void
  LinkManager::PumpLinks()
  {
    for (const auto& link : inboundLinks)
    {
      link->Pump();
    }
    for (const auto& link : outboundLinks)
    {
      link->Pump();
    }
  }

  void
  LinkManager::AddLink(LinkLayer_ptr link, bool inbound)
  {
    util::Lock l(_mutex);

    if (inbound)
    {
      inboundLinks.emplace(link);
    }
    else
    {
      outboundLinks.emplace(link);
    }
  }

  bool
  LinkManager::StartLinks()
  {
    LogInfo("starting ", outboundLinks.size(), " outbound links");
    for (const auto& link : outboundLinks)
    {
      if (!link->Start())
      {
        LogWarn("outbound link '", link->Name(), "' failed to start");
        return false;
      }
      LogDebug("Outbound Link ", link->Name(), " started");
    }

    if (inboundLinks.size())
    {
      LogInfo("starting ", inboundLinks.size(), " inbound links");
      for (const auto& link : inboundLinks)
      {
        if (!link->Start())
        {
          LogWarn("Link ", link->Name(), " failed to start");
          return false;
        }
        LogDebug("Inbound Link ", link->Name(), " started");
      }
    }

    return true;
  }

  void
  LinkManager::Stop()
  {
    if (stopping)
    {
      return;
    }

    util::Lock l(_mutex);

    LogInfo("stopping links");
    stopping = true;

    for (const auto& link : outboundLinks)
      link->Stop();
    for (const auto& link : inboundLinks)
      link->Stop();
  }

  void
  LinkManager::PersistSessionUntil(const RouterID& remote, llarp_time_t until)
  {
    if (stopping)
      return;

    util::Lock l(_mutex);
    auto& curr = m_PersistingSessions[remote];
    if (until > curr)
      curr = until;
    LogDebug("persist session to ", remote, " until ", curr - time_now_ms());
  }

  void
  LinkManager::ForEachPeer(
      std::function<void(const ILinkSession*, bool)> visit, bool randomize) const
  {
    if (stopping)
      return;

    for (const auto& link : outboundLinks)
    {
      link->ForEachSession([visit](const ILinkSession* peer) { visit(peer, true); }, randomize);
    }
    for (const auto& link : inboundLinks)
    {
      link->ForEachSession([visit](const ILinkSession* peer) { visit(peer, false); }, randomize);
    }
  }

  void
  LinkManager::ForEachPeer(std::function<void(ILinkSession*)> visit)
  {
    if (stopping)
      return;

    for (const auto& link : outboundLinks)
    {
      link->ForEachSession([visit](ILinkSession* peer) { visit(peer); });
    }
    for (const auto& link : inboundLinks)
    {
      link->ForEachSession([visit](ILinkSession* peer) { visit(peer); });
    }
  }

  void
  LinkManager::ForEachInboundLink(std::function<void(LinkLayer_ptr)> visit) const
  {
    for (const auto& link : inboundLinks)
    {
      visit(link);
    }
  }

  void
  LinkManager::ForEachOutboundLink(std::function<void(LinkLayer_ptr)> visit) const
  {
    for (const auto& link : outboundLinks)
    {
      visit(link);
    }
  }

  size_t
  LinkManager::NumberOfConnectedRouters() const
  {
    std::set<RouterID> connectedRouters;

    auto fn = [&connectedRouters](const ILinkSession* session, bool) {
      if (session->IsEstablished())
      {
        const RouterContact rc(session->GetRemoteRC());
        if (rc.IsPublicRouter())
        {
          connectedRouters.insert(rc.pubkey);
        }
      }
    };

    ForEachPeer(fn);

    return connectedRouters.size();
  }

  size_t
  LinkManager::NumberOfConnectedClients() const
  {
    std::set<RouterID> connectedClients;

    auto fn = [&connectedClients](const ILinkSession* session, bool) {
      if (session->IsEstablished())
      {
        const RouterContact rc(session->GetRemoteRC());
        if (!rc.IsPublicRouter())
        {
          connectedClients.insert(rc.pubkey);
        }
      }
    };

    ForEachPeer(fn);

    return connectedClients.size();
  }

  size_t
  LinkManager::NumberOfPendingConnections() const
  {
    size_t pending = 0;
    for (const auto& link : inboundLinks)
    {
      pending += link->NumberOfPendingSessions();
    }

    for (const auto& link : outboundLinks)
    {
      pending += link->NumberOfPendingSessions();
    }

    return pending;
  }

  bool
  LinkManager::GetRandomConnectedRouter(RouterContact& router) const
  {
    std::unordered_map<RouterID, RouterContact, RouterID::Hash> connectedRouters;

    ForEachPeer(
        [&connectedRouters](const ILinkSession* peer, bool unused) {
          (void)unused;
          connectedRouters[peer->GetPubKey()] = peer->GetRemoteRC();
        },
        false);

    const auto sz = connectedRouters.size();
    if (sz)
    {
      auto itr = connectedRouters.begin();
      if (sz > 1)
      {
        std::advance(itr, randint() % sz);
      }

      router = itr->second;

      return true;
    }

    return false;
  }

  void
  LinkManager::CheckPersistingSessions(llarp_time_t now)
  {
    if (stopping)
      return;

    std::vector<RouterID> sessionsNeeded;

    {
      util::Lock l(_mutex);

      auto itr = m_PersistingSessions.begin();
      while (itr != m_PersistingSessions.end())
      {
        if (now < itr->second)
        {
          auto link = GetLinkWithSessionTo(itr->first);
          if (link)
          {
            link->KeepAliveSessionTo(itr->first);
          }
          else
          {
            sessionsNeeded.push_back(itr->first);
          }
          ++itr;
        }
        else
        {
          const RouterID r(itr->first);
          LogInfo("commit to ", r, " expired");
          itr = m_PersistingSessions.erase(itr);
          for (const auto& link : outboundLinks)
          {
            link->CloseSessionTo(r);
          }
        }
      }
    }

    for (const auto& router : sessionsNeeded)
    {
      _sessionMaker->CreateSessionTo(router, nullptr);
    }
  }

  void
  LinkManager::updatePeerDb(std::shared_ptr<PeerDb> peerDb)
  {
    std::vector<std::pair<RouterID, SessionStats>> statsToUpdate;

    int64_t diffTotalTX = 0;

    ForEachPeer([&](ILinkSession* session) {
      // derive RouterID
      RouterID id = RouterID(session->GetRemoteRC().pubkey);

      SessionStats sessionStats = session->GetSessionStats();
      SessionStats diff;
      SessionStats& lastStats = m_lastRouterStats[id];

      // TODO: operator overloads / member func for diff
      diff.currentRateRX = std::max(sessionStats.currentRateRX, lastStats.currentRateRX);
      diff.currentRateTX = std::max(sessionStats.currentRateTX, lastStats.currentRateTX);
      diff.totalPacketsRX = sessionStats.totalPacketsRX - lastStats.totalPacketsRX;
      diff.totalAckedTX = sessionStats.totalAckedTX - lastStats.totalAckedTX;
      diff.totalDroppedTX = sessionStats.totalDroppedTX - lastStats.totalDroppedTX;

      diffTotalTX = diff.totalAckedTX + diff.totalDroppedTX + diff.totalInFlightTX;

      lastStats = sessionStats;

      // TODO: if we have both inbound and outbound session, this will overwrite
      statsToUpdate.push_back({id, diff});
    });

    for (auto& routerStats : statsToUpdate)
    {
      peerDb->modifyPeerStats(routerStats.first, [&](PeerStats& stats) {
        // TODO: store separate stats for up vs down
        const auto& diff = routerStats.second;

        // note that 'currentRateRX' and 'currentRateTX' are per-second
        stats.peakBandwidthBytesPerSec = std::max(
            stats.peakBandwidthBytesPerSec,
            (double)std::max(diff.currentRateRX, diff.currentRateTX));
        stats.numPacketsDropped += diff.totalDroppedTX;
        stats.numPacketsSent = diff.totalAckedTX;
        stats.numPacketsAttempted = diffTotalTX;

        // TODO: others -- we have slight mismatch on what we store
      });
    }
  }

  util::StatusObject
  LinkManager::ExtractStatus() const
  {
    std::vector<util::StatusObject> ob_links, ib_links;
    std::transform(
        inboundLinks.begin(),
        inboundLinks.end(),
        std::back_inserter(ib_links),
        [](const auto& link) -> util::StatusObject { return link->ExtractStatus(); });
    std::transform(
        outboundLinks.begin(),
        outboundLinks.end(),
        std::back_inserter(ob_links),
        [](const auto& link) -> util::StatusObject { return link->ExtractStatus(); });

    util::StatusObject obj{{"outbound", ob_links}, {"inbound", ib_links}};

    return obj;
  }

  void
  LinkManager::Init(IOutboundSessionMaker* sessionMaker)
  {
    stopping = false;
    _sessionMaker = sessionMaker;
  }

  LinkLayer_ptr
  LinkManager::GetLinkWithSessionTo(const RouterID& remote) const
  {
    if (stopping)
      return nullptr;

    for (const auto& link : outboundLinks)
    {
      if (link->HasSessionTo(remote))
      {
        return link;
      }
    }
    for (const auto& link : inboundLinks)
    {
      if (link->HasSessionTo(remote))
      {
        return link;
      }
    }
    return nullptr;
  }

}  // namespace llarp
