/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Apr 30, 2021
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#include <ble/cs_Nordic.h>
#include <localisation/cs_MeshTopology.h>
#include <storage/cs_State.h>

#define LOGMeshTopologyInfo  LOGi
#define LOGMeshTopologyDebug LOGvv

cs_ret_code_t MeshTopology::init() {

	State::getInstance().get(CS_TYPE::CONFIG_CROWNSTONE_ID, &_myId, sizeof(_myId));

	_neighbours = new (std::nothrow) neighbour_node_t[MAX_NEIGHBOURS];
	if (_neighbours == nullptr) {
		return ERR_NO_SPACE;
	}
	listen();

#if BUILD_MESH_TOPOLOGY_RESEARCH == 1
	_research.init();
#endif

	return ERR_SUCCESS;
}


cs_ret_code_t MeshTopology::getMacAddress(stone_id_t stoneId) {
	LOGMeshTopologyInfo("getMacAddress %u", stoneId);
	uint8_t index = find(stoneId);
	if (index == INDEX_NOT_FOUND) {
		LOGMeshTopologyInfo("Not a neighbour");
		return ERR_NOT_FOUND;
	}

	cs_mesh_model_msg_stone_mac_t request;
	request.type = 0;

	cs_mesh_msg_t meshMsg;
	meshMsg.type = CS_MESH_MODEL_TYPE_STONE_MAC;
	meshMsg.flags.flags.broadcast = false;
	meshMsg.flags.flags.reliable = true;
	meshMsg.flags.flags.useKnownIds = false;
	meshMsg.reliability = 3; // Low timeout, we expect a result quickly.
	meshMsg.urgency = CS_MESH_URGENCY_LOW;
	meshMsg.idCount = 1;
	meshMsg.targetIds = &stoneId;
	meshMsg.payload = reinterpret_cast<uint8_t*>(&request);
	meshMsg.size = sizeof(request);

	event_t event(CS_TYPE::CMD_SEND_MESH_MSG, &meshMsg, sizeof(meshMsg));
	event.dispatch();
	LOGMeshTopologyInfo("Sent mesh msg retCode=%u", event.result.returnCode);
	if (event.result.returnCode != ERR_SUCCESS) {
		return event.result.returnCode;
	}

	return ERR_WAIT_FOR_SUCCESS;
}


void MeshTopology::add(stone_id_t id, int8_t rssi, uint8_t channel) {
	if (id == 0 || id == _myId) {
		return;
	}
	uint8_t compressedRssi = compressRssi(rssi);
	uint8_t compressedChannel = compressChannel(channel);
	uint8_t index = find(id);
	if (index == INDEX_NOT_FOUND) {
		if (_neighbourCount < MAX_NEIGHBOURS) {
			_neighbours[_neighbourCount] = neighbour_node_t(id, compressedRssi, compressedChannel);
			_neighbourCount++;
		}
		else {
			LOGw("Can't add id=%u", id);
		}
	}
	else {
		_neighbours[index].rssi = compressedRssi;
		_neighbours[index].channel = compressedChannel;
		_neighbours[index].lastSeenSeconds = 0;
	}
}

uint8_t MeshTopology::find(stone_id_t id) {
	for (uint8_t index = 0; index < _neighbourCount; ++index) {
		if (_neighbours[index].id == id) {
			return index;
		}
	}
	return INDEX_NOT_FOUND;
}

cs_ret_code_t MeshTopology::onStoneMacMsg(stone_id_t id, cs_mesh_model_msg_stone_mac_t& packet, mesh_reply_t* reply) {
	switch (packet.type) {
		case 0: {
			LOGMeshTopologyInfo("Send mac address");

			if (reply == nullptr) {
				return ERR_BUFFER_UNASSIGNED;
			}
			if (reply->buf.len < sizeof(cs_mesh_model_msg_stone_mac_t)) {
				return ERR_BUFFER_TOO_SMALL;
			}
			cs_mesh_model_msg_stone_mac_t* replyPacket = reinterpret_cast<cs_mesh_model_msg_stone_mac_t*>(reply->buf.data);
			replyPacket->type = 1;

			ble_gap_addr_t address;
			if (sd_ble_gap_addr_get(&address) != NRF_SUCCESS) {
				return ERR_UNSPECIFIED;
			}
			memcpy(replyPacket->mac, address.addr, MAC_ADDRESS_LEN);
			reply->type = CS_MESH_MODEL_TYPE_STONE_MAC;
			reply->dataSize = sizeof(cs_mesh_model_msg_stone_mac_t);
			break;
		}
		case 1: {
			LOGMeshTopologyInfo("Received mac address id=%u mac=%02X:%02X:%02X:%02X:%02X:%02X", id, packet.mac[5], packet.mac[4], packet.mac[3], packet.mac[2], packet.mac[1], packet.mac[0]);
			TYPIFY(EVT_MESH_TOPO_MAC_RESULT) result;
			result.stoneId = id;
			memcpy(result.macAddress, packet.mac, sizeof(result.macAddress));
			event_t event(CS_TYPE::EVT_MESH_TOPO_MAC_RESULT, &result, sizeof(result));
			event.dispatch();
			break;
		}
	}
	return ERR_SUCCESS;
}

void MeshTopology::onMeshMsg(MeshMsgEvent& packet, cs_result_t& result) {
	if (packet.type == CS_MESH_MODEL_TYPE_STONE_MAC) {
		cs_mesh_model_msg_stone_mac_t payload = packet.getPacket<CS_MESH_MODEL_TYPE_STONE_MAC>();
		result.returnCode = onStoneMacMsg(packet.srcAddress, payload, packet.reply);
	}

	if (packet.hops != 0) {
		return;
	}
	add(packet.srcAddress, packet.rssi, packet.channel);
}

void MeshTopology::onTickSecond() {
	LOGMeshTopologyDebug("onTick");
	print();
	[[maybe_unused]] bool change = false;
	for (uint8_t i = 0; i < _neighbourCount; /**/ ) {
		_neighbours[i].lastSeenSeconds++;
		if (_neighbours[i].lastSeenSeconds == TIMEOUT_SECONDS) {
			change = true;
			// Remove item, by shifting all items after this item.
			_neighbourCount--;
			for (uint8_t j = i; j < _neighbourCount; ++j) {
				_neighbours[j] = _neighbours[j + 1];
			}
		}
		else {
			i++;
		}
	}
	if (change) {
		LOGMeshTopologyDebug("result");
		print();
	}
}

uint8_t MeshTopology::compressRssi(int8_t rssi) {
	if (rssi > -37) {
		return 63;
	}
	if (rssi < -100) {
		return 0;
	}
	return 100 + rssi;
}

uint8_t MeshTopology::compressChannel(uint8_t channel) {
	switch (channel) {
		case 37: return 1;
		case 38: return 2;
		case 39: return 3;
		default: return 0;
	}
}

void MeshTopology::print() {
	for (uint8_t i = 0; i < _neighbourCount; ++i) {
		LOGMeshTopologyDebug("id=%u rssi=%i channel=%u lastSeen=%u", _neighbours[i].id, _neighbours[i].rssi, _neighbours[i].channel, _neighbours[i].lastSeenSeconds);
	}
}

void MeshTopology::handleEvent(event_t &evt) {
	switch (evt.type) {
		case CS_TYPE::EVT_RECV_MESH_MSG: {
			auto packet = CS_TYPE_CAST(EVT_RECV_MESH_MSG, evt.data);
			onMeshMsg(*packet, evt.result);
			break;
		}
		case CS_TYPE::EVT_TICK: {
			auto packet = CS_TYPE_CAST(EVT_TICK, evt.data);
			if (*packet % (1000 / TICK_INTERVAL_MS) == 0) {
				onTickSecond();
			}
			break;
		}
		case CS_TYPE::CMD_MESH_TOPO_GET_MAC: {
			auto packet = CS_TYPE_CAST(CMD_MESH_TOPO_GET_MAC, evt.data);
			evt.result.returnCode = getMacAddress(*packet);
			break;
		}
		default:
			break;
	}
}
