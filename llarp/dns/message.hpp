#ifndef LLARP_DNS_MESSAGE_HPP
#define LLARP_DNS_MESSAGE_HPP

#include <dns/serialize.hpp>
#include <dns/rr.hpp>
#include <dns/question.hpp>

namespace llarp
{
  namespace dns
  {
    struct SRVData;

    using MsgID_t = uint16_t;
    using Fields_t = uint16_t;
    using Count_t = uint16_t;

    struct MessageHeader : public Serialize
    {
      static constexpr size_t Size = 12;

      MessageHeader() = default;

      MsgID_t id;
      Fields_t fields;
      Count_t qd_count;
      Count_t an_count;
      Count_t ns_count;
      Count_t ar_count;

      bool
      Encode(llarp_buffer_t* buf) const override;

      bool
      Decode(llarp_buffer_t* buf) override;

      bool
      operator==(const MessageHeader& other) const
      {
        return id == other.id && fields == other.fields && qd_count == other.qd_count
            && an_count == other.an_count && ns_count == other.ns_count
            && ar_count == other.ar_count;
      }
    };

    struct Message : public Serialize
    {
      Message(const MessageHeader& hdr);

      Message(Message&& other);
      Message(const Message& other);

      void
      AddNXReply(RR_TTL_t ttl = 1);

      void
      AddServFail(RR_TTL_t ttl = 30);

      void
      AddMXReply(std::string name, uint16_t priority, RR_TTL_t ttl = 1);

      void
      AddCNAMEReply(std::string name, RR_TTL_t ttl = 1);

      void
      AddINReply(llarp::huint128_t addr, bool isV6, RR_TTL_t ttl = 1);

      void
      AddAReply(std::string name, RR_TTL_t ttl = 1);

      void
      AddSRVReply(std::vector<SRVData> records, RR_TTL_t ttl = 1);

      void
      AddNSReply(std::string name, RR_TTL_t ttl = 1);

      void
      AddTXTReply(std::string value, RR_TTL_t ttl = 1);

      bool
      Encode(llarp_buffer_t* buf) const override;

      bool
      Decode(llarp_buffer_t* buf) override;

      // Wrapper around Encode that encodes into a new buffer and returns it
      [[nodiscard]] OwnedBuffer
      ToBuffer() const;

      std::ostream&
      print(std::ostream& stream, int level, int spaces) const;

      MsgID_t hdr_id;
      Fields_t hdr_fields;
      std::vector<Question> questions;
      std::vector<ResourceRecord> answers;
      std::vector<ResourceRecord> authorities;
      std::vector<ResourceRecord> additional;
    };

    inline std::ostream&
    operator<<(std::ostream& out, const Message& msg)
    {
      msg.print(out, -1, -1);
      return out;
    }
  }  // namespace dns
}  // namespace llarp

#endif
