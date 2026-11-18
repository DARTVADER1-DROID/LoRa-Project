#pragma once
#include "types.hpp"
namespace ca {
class IRadioTransport{public:virtual ~IRadioTransport(){};virtual Status begin(const RadioProfile&)=0;virtual Status send(const ProtocolMessage&)=0;virtual bool pollReceived(ReceivedFrame&)=0;virtual void service()=0;};
class IButtonInput{public:virtual ~IButtonInput(){};virtual Status begin()=0;virtual void service(uint32_t)=0;virtual bool pollEvent(ButtonEvent&)=0;};
class IDisplay{public:virtual ~IDisplay(){};virtual Status begin()=0;virtual void render(const UiViewModel&)=0;};
class IBuzzer{public:virtual ~IBuzzer(){};virtual Status begin()=0;virtual void playCue(uint8_t)=0;virtual void service(uint32_t)=0;};
class IClock{public:virtual ~IClock(){};virtual uint32_t millisNow()const=0;};
class IPersistence{public:virtual ~IPersistence(){};virtual Status load(uint16_t,uint8_t*,uint16_t)=0;virtual Status storeAtomically(uint16_t,const uint8_t*,uint16_t)=0;virtual Status erase(uint16_t)=0;};
class IDiagnostics{public:virtual ~IDiagnostics(){};virtual void record(uint16_t,ErrorCode)=0;virtual void increment(uint16_t)=0;};
}
