#include "pathfinder/profile/trapezoidal.h"
#include "pathfinder/math.h"

void Pathfinder::Profile::Trapezoidal::configure(float max_velocity, float acceleration) {
    _max_velocity = max_velocity;
    _acceleration = acceleration;
}

void Pathfinder::Profile::Trapezoidal::configure_shift(ShiftLevel *levels, int level_count) {
    _slvls = levels;
    _slcount = level_count;
    _slconfigured = true;

    set_shift(0);
}

int Pathfinder::Profile::Trapezoidal::shift_level() {
    return _slcurrent;
}

void Pathfinder::Profile::Trapezoidal::set_shift(int level) {
    _slcurrent = level;
    _max_velocity = _slvls[_slcurrent].max_velocity;
    _acceleration = _slvls[_slcurrent].acceleration;
}

uint8_t Pathfinder::Profile::Trapezoidal::calculate(Pathfinder::Segment *segment_out, Pathfinder::Segment *last_segment, float time) {
    // Will get destroyed at end of scope. Placed here in the event last_segment is nullptr (i.e. start of generation),
    // we make an assumption of position 0, velocity 0, acceleration 0 and time 0.
    Pathfinder::Segment zero_seg = { 0,0,0,0 };
    if (last_segment == nullptr) last_segment = &zero_seg;
    uint8_t status = Pathfinder::Profile::STATUS_LEVEL;

    // Store these incase segment_out and last_segment point to the same memory address (in cases
    // where all generation is done on the fly)
    float   l_time = last_segment->time,
            l_dist = last_segment->distance,
            l_vel = last_segment->velocity,
            l_acc = last_segment->acceleration;
    
    segment_out->time = time;

    // t = v/a
    // s = ut + 0.5at^2
    // s = l_vel*t - 0.5*accel*t*t
    float   decel_time = l_vel / _acceleration,
            decel_dist = l_vel * decel_time - 0.5*_acceleration*decel_time*decel_time;

    float   dt = time - l_time;

    if (l_dist >= _setpoint) {
        // Drop all to 0 and return
        segment_out->acceleration = 0;
        segment_out->velocity = 0;
        segment_out->distance = l_dist;
        status = Pathfinder::Profile::STATUS_DONE;
    }

    if (l_dist + decel_dist >= _setpoint) {
        // If we start decelerating now, we'll hit the setpoint
        // We're in the 'speed down' section of the profile
        // TODO: The Deceleration Case
        segment_out->acceleration = -_acceleration;
        // v = u - at
        segment_out->velocity = l_vel - (_acceleration * dt);
        // s = s0 + ut - 0.5at^2
        segment_out->distance = l_dist + (l_vel * dt) - (0.5 * _acceleration * dt * dt);
        status = Pathfinder::Profile::STATUS_DECEL;
    }

    if (l_vel < _max_velocity) {
        // We're in the 'speed up' section of the profile
        segment_out->acceleration = _acceleration;
        // v = u + at
        segment_out->velocity = MIN(l_vel + (_acceleration * dt), _max_velocity);
        // s = s0 + ut + 0.5at^2
        segment_out->distance = l_dist + (l_vel * dt) + (0.5 * _acceleration * dt * dt);
        status = Pathfinder::Profile::STATUS_ACCEL;
    }

    if (status == Pathfinder::Profile::STATUS_LEVEL) {
        // If nothing else, we have levelled out, hold steady velocity
        segment_out->acceleration = 0;
        segment_out->velocity = l_vel;
        segment_out->distance = l_dist + (l_vel * dt);
    } else if (_slconfigured) {
        float cur_v = segment_out->velocity;
        if (_slcurrent + 1 < _slcount) {
            ShiftLevel *up = (_slvls + _slcurrent + 1);
            if (cur_v > up->threshold_velocity) {
                set_shift(++_slcurrent);
            }
        }
        
        if (_slcurrent > 0) {
            if (cur_v < _slvls[_slcurrent].threshold_velocity) {
                set_shift(--_slcurrent);
            }
        }
    }
    return status;
}