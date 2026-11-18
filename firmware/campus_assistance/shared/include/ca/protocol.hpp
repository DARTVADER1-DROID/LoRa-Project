#pragma once
#include "types.hpp"
namespace ca {
struct EncodeResult{Status status;uint16_t bytesWritten;};struct DecodeResult{Status status;uint16_t bytesRead;};
class FrameCodec{public:EncodeResult encode(const ProtocolMessage&,uint8_t*,uint16_t)const;DecodeResult decode(const uint8_t*,uint16_t,ProtocolMessage&)const;};
class MessageValidator{public:Status validate(const ProtocolMessage&,const DeviceConfig&)const;};
class ProtocolEndpoint{public:Status begin();void service(uint32_t);Status enqueue(const ProtocolMessage&);bool pollEvent(ProtocolMessage&);};
class RetryManager{public:Status schedule(const ProtocolMessage&,TransactionId,uint32_t);void service(uint32_t);void onAcknowledgement(TransactionId);void cancel(TransactionId);};
}
