#pragma once

#include <llarp/path/pathbuilder.hpp>
#include "sendcontext.hpp"
#include <llarp/util/status.hpp>

#include <unordered_map>
#include <unordered_set>

namespace llarp
{
  namespace service
  {
    struct AsyncKeyExchange;
    struct Endpoint;

    /// context needed to initiate an outbound hidden service session
    struct OutboundContext : public path::Builder,
                             public SendContext,
                             public std::enable_shared_from_this<OutboundContext>
    {
      OutboundContext(const IntroSet& introSet, Endpoint* parent);

      ~OutboundContext() override;

      util::StatusObject
      ExtractStatus() const;

      void
      BlacklistSNode(const RouterID) override{};

      bool
      ShouldBundleRC() const override;

      path::PathSet_ptr
      GetSelf() override
      {
        return shared_from_this();
      }

      bool
      Stop() override;

      bool
      HandleDataDrop(path::Path_ptr p, const PathID_t& dst, uint64_t s);

      void
      HandlePathDied(path::Path_ptr p) override;

      /// set to true if we are updating the remote introset right now
      bool updatingIntroSet;

      /// update the current selected intro to be a new best introduction
      /// return true if we have changed intros
      bool
      ShiftIntroduction(bool rebuild = true) override;

      /// shift the intro off the current router it is using
      void
      ShiftIntroRouter(const RouterID remote);

      /// mark the current remote intro as bad
      bool
      MarkCurrentIntroBad(llarp_time_t now) override;

      bool
      MarkIntroBad(const Introduction& marked, llarp_time_t now);

      /// return true if we are ready to send
      bool
      ReadyToSend() const;

      /// for exits
      void
      SendPacketToRemote(const llarp_buffer_t&) override;

      bool
      ShouldBuildMore(llarp_time_t now) const override;

      /// pump internal state
      /// return true to mark as dead
      bool
      Pump(llarp_time_t now);

      /// return true if it's safe to remove ourselves
      bool
      IsDone(llarp_time_t now) const;

      bool
      CheckPathIsDead(path::Path_ptr p, llarp_time_t dlt);

      void
      AsyncGenIntro(const llarp_buffer_t& payload, ProtocolType t) override;

      /// issues a lookup to find the current intro set of the remote service
      void
      UpdateIntroSet() override;

      void
      HandlePathBuilt(path::Path_ptr path) override;

      void
      HandlePathBuildTimeout(path::Path_ptr path) override;

      void
      HandlePathBuildFailed(path::Path_ptr path) override;

      std::optional<std::vector<RouterContact>>
      GetHopsForBuild() override;

      bool
      HandleHiddenServiceFrame(path::Path_ptr p, const ProtocolFrame& frame);

      std::string
      Name() const override;

      const IntroSet&
      GetCurrentIntroSet() const
      {
        return currentIntroSet;
      }

     private:
      /// swap remoteIntro with next intro
      void
      SwapIntros();

      void
      OnGeneratedIntroFrame(AsyncKeyExchange* k, PathID_t p);

      bool
      OnIntroSetUpdate(const Address& addr, std::optional<IntroSet> i, const RouterID& endpoint);

      const dht::Key_t location;
      uint64_t m_UpdateIntrosetTX = 0;
      IntroSet currentIntroSet;
      Introduction m_NextIntro;
      std::unordered_map<Introduction, llarp_time_t, Introduction::Hash> m_BadIntros;
      llarp_time_t lastShift = 0s;
      uint16_t m_LookupFails = 0;
      uint16_t m_BuildFails = 0;
      llarp_time_t m_LastInboundTraffic = 0s;
      bool m_GotInboundTraffic = false;
    };
  }  // namespace service

}  // namespace llarp
