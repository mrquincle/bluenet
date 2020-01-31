/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Jan 28, 2020
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#include <storage/cs_StateData.h>
#include <storage/cs_State.h>
#include <switch/cs_SmartSwitch.h>

#define LOGSmartSwitch LOGd

void SmartSwitch::init(const boards_config_t& board) {
	State::getInstance().get(CS_TYPE::CONFIG_PWM_ALLOWED, &allowDimming, sizeof(allowDimming));

	TYPIFY(CONFIG_SWITCH_LOCKED) switchLocked;
	State::getInstance().get(CS_TYPE::CONFIG_SWITCH_LOCKED, &switchLocked, sizeof(switchLocked));
	allowSwitching = !switchLocked;

	// Load intended state.
	State::getInstance().get(CS_TYPE::STATE_SWITCH_STATE, &storedState, sizeof(storedState));
	uint8_t intendedState = getIntensityFromSwitchState(storedState);
	LOGi("init intensity=%u allowDimming=%u allowSwitching=%u", intendedState, allowDimming, allowSwitching);


	safeSwitch.onUnexpextedStateChange([&](switch_state_t newState) -> void {
		handleUnexpectedStateChange(newState);
	});

	safeSwitch.init(board);

	listen();

	// Since the safeSwitch is initialized, it will already have its state loaded.
	set(intendedState);
}

void SmartSwitch::start() {
	LOGd("start");
	safeSwitch.start();
}

switch_state_t SmartSwitch::getActualState() {
	return safeSwitch.getState();
}

uint8_t SmartSwitch::getIntensityFromSwitchState(switch_state_t switchState) {
	if (switchState.state.relay) {
		return 100;
	}
	return switchState.state.dimmer;
}

uint8_t SmartSwitch::getCurrentIntensity() {
	auto currentState = getActualState();
	return getIntensityFromSwitchState(currentState);
}

uint8_t SmartSwitch::getIntendedState() {
	return intendedState;
}

cs_ret_code_t SmartSwitch::set(uint8_t intensity) {
	LOGSmartSwitch("set %u", intensity);
	if (intensity > 100) {
		intensity = 100;
//		return ERR_WRONG_PARAMETER;
	}
	intendedState = intensity;
	return resolveIntendedState();
}

cs_ret_code_t SmartSwitch::resolveIntendedState() {
	switch_state_t currentState = getActualState();
	LOGSmartSwitch("resolveIntendedState intended=%u current=(%u %u%%)", intendedState, currentState.state.relay, currentState.state.dimmer);

	if (intendedState == 0) {
		// Set dimmer to 0 and turn relay off.
		cs_ret_code_t retCode = ERR_SUCCESS;
		cs_ret_code_t retCodeDimmer = setDimmer(0, currentState);
		if (retCodeDimmer != ERR_SUCCESS) {
			retCode = retCodeDimmer;
		}
		cs_ret_code_t retCodeRelay = setRelay(false, currentState);
		if (retCodeRelay != ERR_SUCCESS) {
			retCode = retCodeRelay;
		}
		return retCode;
	}
	else if (intendedState == 100) {
		// Set dimmer to 100 or turn relay on.
		if (currentState.state.relay || currentState.state.dimmer == 100) {
			return ERR_SUCCESS;
		}
		cs_ret_code_t retCode = ERR_UNSPECIFIED;
		if (allowDimming) {
			retCode = setDimmer(100, currentState);
		}
		else {
			retCode = setRelay(true, currentState);
		}
		return retCode;
	}
	else {
		// Try to set dimmed value, and then turn relay off.
		// If that doesn't work, turn on relay instead.
		cs_ret_code_t retCode = setDimmer(intendedState, currentState);
		currentState = getActualState();
		if (retCode == ERR_SUCCESS) {
			retCode = setRelay(false, currentState);
			if (retCode != ERR_SUCCESS) {
				// If relay was not turned off: turn dimmer off.
				setDimmer(0, currentState);
			}
		}
		else {
			setRelay(true, currentState);
		}
		return retCode;
	}
}

cs_ret_code_t SmartSwitch::setRelay(bool on, switch_state_t currentState) {
	LOGSmartSwitch("setRelay %u currenState=%u allowSwitching=%u", on, currentState.asInt, allowSwitching);
	if (currentState.state.relay == on) {
		return ERR_SUCCESS;
	}
	if (!allowSwitching) {
		return ERR_NO_ACCESS;
	}
	return setRelayUnchecked(on);
}

cs_ret_code_t SmartSwitch::setRelayUnchecked(bool on) {
	cs_ret_code_t retCode = safeSwitch.setRelay(on);
	LOGSmartSwitch("setRelayUnchecked %u retCode=%u", on, retCode);
	switch_state_t currentState = getActualState();
	store(currentState);
	return retCode;
}

cs_ret_code_t SmartSwitch::setDimmer(uint8_t intensity, switch_state_t currentState) {
	LOGSmartSwitch("setDimmer %u currenState=%u allowSwitching=%u allowDimming=%u", intensity, currentState.asInt, allowSwitching, allowDimming);
	if (currentState.state.dimmer == intensity) {
		return ERR_SUCCESS;
	}
	if (!allowSwitching) {
		return ERR_NO_ACCESS;
	}
	if (!allowDimming && intensity > 0) {
		return ERR_NO_ACCESS;
	}
	return setDimmerUnchecked(intensity);
}

cs_ret_code_t SmartSwitch::setDimmerUnchecked(uint8_t intensity) {
	cs_ret_code_t retCode = safeSwitch.setDimmer(intensity);
	LOGSmartSwitch("setDimmerUnchecked %u retCode=%u", intensity, retCode);
	switch_state_t currentState = getActualState();
	store(currentState);
	return retCode;
}



void SmartSwitch::handleUnexpectedStateChange(switch_state_t newState) {
	uint8_t intensity = getIntensityFromSwitchState(newState);
	sendUnexpectedIntensityUpdate(intensity);
}

void SmartSwitch::onUnexpextedIntensityChange(const callback_on_intensity_change_t& closure) {
	callbackOnIntensityChange = closure;
}

void SmartSwitch::sendUnexpectedIntensityUpdate(uint8_t newIntensity) {
	LOGd("sendUnexpectedIntensityUpdate");
	callbackOnIntensityChange(newIntensity);
}


void SmartSwitch::store(switch_state_t newState) {
	if (newState.asInt == storedState.asInt) {
		return;
	}
	LOGd("store(%u, %u%%)", newState.state.relay, newState.state.dimmer);
	storedState = newState;

	bool persistNow = false;
	cs_state_data_t stateData(CS_TYPE::STATE_SWITCH_STATE, reinterpret_cast<uint8_t*>(&newState), sizeof(newState));
	if (persistNow) {
		State::getInstance().set(stateData);
	}
	else {
		State::getInstance().setDelayed(stateData, SWITCH_DELAYED_STORE_MS / 1000);
	}
}



cs_ret_code_t SmartSwitch::setAllowSwitching(bool allowed) {
	LOGi("setAllowSwitching %u", allowed);
	allowSwitching = allowed;
	TYPIFY(CONFIG_SWITCH_LOCKED) switchLocked = !allowSwitching;
	State::getInstance().set(CS_TYPE::CONFIG_SWITCH_LOCKED, &switchLocked, sizeof(switchLocked));
	return ERR_SUCCESS;
}

cs_ret_code_t SmartSwitch::setAllowDimming(bool allowed) {
	LOGi("setAllowDimming %u", allowed);
	allowDimming = allowed;
	State::getInstance().set(CS_TYPE::CONFIG_SWITCH_LOCKED, &allowDimming, sizeof(allowDimming));
	// Update the switch state, as it may need to be changed.
	[[maybe_unused]] cs_ret_code_t retCode = resolveIntendedState();
	uint8_t newIntensity = getCurrentIntensity();
	if (newIntensity != intendedState) {
		sendUnexpectedIntensityUpdate(newIntensity);
	}
	return ERR_SUCCESS;
}

cs_ret_code_t SmartSwitch::handleCommandSetRelay(bool on) {
	LOGi("handleCommandSetRelay %u", on);
	[[maybe_unused]] cs_ret_code_t retCode = setRelay(on, getActualState());
	uint8_t newIntensity = getCurrentIntensity();
	if (newIntensity != intendedState) {
		sendUnexpectedIntensityUpdate(newIntensity);
	}
	return retCode;
}

cs_ret_code_t SmartSwitch::handleCommandSetDimmer(uint8_t intensity) {
	LOGi("handleCommandSetDimmer %u", intensity);
	[[maybe_unused]] cs_ret_code_t retCode = setDimmer(intensity, getActualState());
	uint8_t newIntensity = getCurrentIntensity();
	if (newIntensity != intendedState) {
		sendUnexpectedIntensityUpdate(newIntensity);
	}
	return retCode;
}


void SmartSwitch::handleEvent(event_t& evt) {
	switch (evt.type) {
		case CS_TYPE::CMD_SWITCHING_ALLOWED: {
			auto allowed = reinterpret_cast<TYPIFY(CMD_SWITCHING_ALLOWED)*>(evt.data);
			evt.result.returnCode = setAllowSwitching(*allowed);
			break;
		}
		case CS_TYPE::CMD_DIMMING_ALLOWED: {
			auto allowed = reinterpret_cast<TYPIFY(CMD_DIMMING_ALLOWED)*>(evt.data);
			evt.result.returnCode = setAllowDimming(*allowed);
			break;
		}
		case CS_TYPE::CMD_SET_RELAY: {
			auto turnOn = reinterpret_cast<TYPIFY(CMD_SET_RELAY)*>(evt.data);
			evt.result.returnCode = handleCommandSetRelay(*turnOn);
			break;
		}
		case CS_TYPE::CMD_SET_DIMMER: {
			auto intensity = reinterpret_cast<TYPIFY(CMD_SET_DIMMER)*>(evt.data);
			evt.result.returnCode = handleCommandSetDimmer(*intensity);
			break;
		}
		case CS_TYPE::EVT_DIMMER_POWERED: {
			// Fade to intended intensity?
			break;
		}
		default:
			break;
	}
}