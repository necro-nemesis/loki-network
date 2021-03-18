#include "hidden_service_address_lookup.hpp"

#include <llarp/dht/messages/findintro.hpp>
#include "endpoint.hpp"
#include <utility>

namespace llarp
{
  namespace service
  {
    HiddenServiceAddressLookup::HiddenServiceAddressLookup(
        Endpoint* p,
        HandlerFunc h,
        const dht::Key_t& l,
        const PubKey& k,
        uint64_t order,
        uint64_t tx)
        : IServiceLookup(p, tx, "HSLookup")
        , rootkey(k)
        , relayOrder(order)
        , location(l)
        , handle(std::move(h))
    {}

    bool
    HiddenServiceAddressLookup::HandleIntrosetResponse(const std::set<EncryptedIntroSet>& results)
    {
      std::optional<IntroSet> found;
      const Address remote(rootkey);
      if (results.size() > 0)
      {
        EncryptedIntroSet selected;
        for (const auto& introset : results)
        {
          if (selected.OtherIsNewer(introset))
            selected = introset;
        }
        const auto maybe = selected.MaybeDecrypt(rootkey);
        if (maybe)
        {
          LogInfo("found result for ", remote.ToString());
          found = *maybe;
        }
      }
      return handle(remote, found, endpoint);
    }

    std::shared_ptr<routing::IMessage>
    HiddenServiceAddressLookup::BuildRequestMessage()
    {
      auto msg = std::make_shared<routing::DHTMessage>();
      msg->M.emplace_back(std::make_unique<dht::FindIntroMessage>(txid, location, relayOrder));
      return msg;
    }

  }  // namespace service
}  // namespace llarp
