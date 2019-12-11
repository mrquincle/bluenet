/**
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Sep 24, 2019
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#include <util/cs_WireFormat.h>

#include <behaviour/cs_Behaviour.h>
#include <behaviour/cs_SwitchBehaviour.h>
#include <behaviour/cs_TwilightBehaviour.h>

namespace WireFormat {

// -------------------- specialization for deserialize --------------------

template<> 
uint8_t WireFormat::deserialize(uint8_t* data, size_t len){
    // TODO(Arend): assert length
    return data[0];
}

template<> 
uint32_t WireFormat::deserialize(uint8_t* data, size_t len){
    // TODO(Arend): assert length
    return data[3] << 8*3 | data[2] << 8*2 | data[1] << 8*1 | data[0]  << 8*0;
}

template<> 
int32_t WireFormat::deserialize(uint8_t* data, size_t len){
    // TODO(Arend): assert length
    return data[3] << 8*3 | data[2] << 8*2 | data[1] << 8*1 | data[0]  << 8*0;
}

template<> 
uint64_t WireFormat::deserialize(uint8_t* data, size_t len){
    // TODO(Arend): assert length
    return 
        static_cast<uint64_t>(data[7]) << 8*7 | 
        static_cast<uint64_t>(data[6]) << 8*6 | 
        static_cast<uint64_t>(data[5]) << 8*5 | 
        static_cast<uint64_t>(data[4]) << 8*4 |
        static_cast<uint64_t>(data[3]) << 8*3 | 
        static_cast<uint64_t>(data[2]) << 8*2 | 
        static_cast<uint64_t>(data[1]) << 8*1 | 
        static_cast<uint64_t>(data[0]) << 8*0;
}

template<>
TimeOfDay WireFormat::deserialize(uint8_t* data, size_t len){
    // TODO(Arend): assert length
    std::array<uint8_t,5> d;
    std::copy_n(data, 5, d.begin());
    return TimeOfDay(d);
}

template<>
PresencePredicate WireFormat::deserialize(uint8_t* data, size_t len){
    // TODO(Arend): assert length
    std::array<uint8_t,9> d;
    std::copy_n(data, 9, d.begin());
    return PresencePredicate(d);
}

template<>
PresenceCondition WireFormat::deserialize(uint8_t* data, size_t len){
    // TODO(Arend): assert length
    std::array<uint8_t,13> d;
    std::copy_n(data, 13, d.begin());
    return PresenceCondition(d);
}

template<>
Behaviour WireFormat::deserialize(uint8_t* data, size_t len){
    // TODO(Arend): assert length
    std::array<uint8_t,1+13> d;
    std::copy_n(data, 1+13, d.begin());
    return Behaviour(d);
}

template<>
SwitchBehaviour WireFormat::deserialize(uint8_t* data, size_t len){
    // TODO(Arend): assert length
    std::array<uint8_t,1+26> d;
    std::copy_n(data, 1+26, d.begin());
    return SwitchBehaviour(d);
}

template<>
TwilightBehaviour WireFormat::deserialize(uint8_t* data, size_t len){
    // TODO(Arend): assert length
    constexpr auto serialized_size = size<TwilightBehaviour>();
    std::array<uint8_t,serialized_size> d;
    std::copy_n(data, serialized_size, d.begin());
    return TwilightBehaviour(d);
}

// -------------------- specialization for serialize --------------------


std::array<uint8_t,1> serialize(const uint8_t& obj){
    LOGWireFormat("serialize uint8_t");
    return {obj};
}

std::array<uint8_t,4> serialize(const uint32_t& obj){
    LOGWireFormat("serialize uint32_t");
    return {
        static_cast<uint8_t>(obj >> 0), 
        static_cast<uint8_t>(obj >> 1), 
        static_cast<uint8_t>(obj >> 2), 
        static_cast<uint8_t>(obj >> 3)};
}

std::array<uint8_t,4> serialize(const int32_t& obj){
    LOGWireFormat("serialize int32_t");
    return {
        static_cast<uint8_t>(obj >> 0), 
        static_cast<uint8_t>(obj >> 1), 
        static_cast<uint8_t>(obj >> 2), 
        static_cast<uint8_t>(obj >> 3)};
}

std::array<uint8_t,8> serialize(const uint64_t& obj){
    LOGWireFormat("serialize uint64_t");
    return {
        static_cast<uint8_t>(obj >> 0), 
        static_cast<uint8_t>(obj >> 1), 
        static_cast<uint8_t>(obj >> 2), 
        static_cast<uint8_t>(obj >> 3),
        static_cast<uint8_t>(obj >> 4), 
        static_cast<uint8_t>(obj >> 5), 
        static_cast<uint8_t>(obj >> 6), 
        static_cast<uint8_t>(obj >> 7)};
}

} // namespace WireFormat
