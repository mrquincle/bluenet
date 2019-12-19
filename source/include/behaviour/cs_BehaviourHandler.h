/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Dec 20, 2019
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */
#pragma once

#include <behaviour/cs_SwitchBehaviour.h>
#include <events/cs_EventListener.h>

#include <optional>


class BehaviourHandler : public EventListener {
    public:

    /**
     * Computes the intended behaviour state of this crownstone based on
     * the stored behaviours, and then dispatches an event for that.
     * 
     * Events:
     * - STATE_TIME
     * - EVT_TIME_SET
     * - EVT_PRESENCE_MUTATION
     * - EVT_BEHAVIOURSTORE_MUTATION
     */
    virtual void handleEvent(event_t& evt);

    /**
     * Acquires the current time and presence information. 
     * Checks the intended state by looping over the active behaviours
     * and if the intendedState differs from previousIntendedState
     * dispatch an event to communicate a state update.
     */
    bool update();

    std::optional<uint8_t> getValue();

    private:

    /**
     * Given current time/presence, query the behaviourstore and check
     * if there any valid ones. 
     * 
     * Returns an empty optional when this BehaviourHandler is inactive, or 
     * more than one valid behaviours contradicted eachother.
     * Returns a non-empty optional if a valid behaviour is found or
     * multiple agreeing behaviours have been found.
     * In this case its value contains the desired state value.
     * When no behaviours are valid at given time/presence the intended
     * value is 0. (house is 'off' by default)
     */
    std::optional<uint8_t> computeIntendedState(
        TimeOfDay currenttime, 
        PresenceStateDescription currentpresence);

    std::optional<uint8_t> previousIntendedState = {};

    /******
     * setting this to false will result in a BehaviourHandler that will
     * not have an opinion about the state anymore (getValue returns std::nullopt).
     */
    bool isActive = true;
};
