#pragma once
#include "../motorlib/control_fun.h"
#include <string>

class TensionProgram {
 public:
    enum State {OFF, LOW_VELOCITY, START_VELOCITY, TORQUE, ERROR};
    const std::string state_string[5] = {"OFF", "LOW_VELOCITY", "START_VELOCITY", "TORQUE", "ERROR"};
    std::string get_state() const { return state_string[state_]; }
    void loop() {
        if (config::main_loop.first_command_received()) {
            // exit this program
            if (command_received_ == false) {
                System::log("received external command, tension program exiting");
                command_received_ = true;
            }
            return;
        }

        t_ms_++;
        status_ = config::main_loop.get_status();
        config::gpio1.update();     // for deboucing
        config::gpio2.update();
        float dt = (status_.fast_loop.timestamp - last_timestamp_)*(1.0/CPU_FREQUENCY_HZ);
        float velocity = (status_.fast_loop.motor_position.position - last_motor_position_)/dt;
        last_timestamp_ = status_.fast_loop.timestamp;
        last_motor_position_ = status_.fast_loop.motor_position.position;
        float velocity_filtered = velocity_filter_.update(velocity);
        float torque_filtered = torque_filter_.update(status_.torque);

        MotorCommand command = {};
        switch (state_) {
            case LOW_VELOCITY:
                command.mode_desired = VELOCITY;
                command.velocity_desired = 10;
                if (config::gpio1.is_clear()) { // button pushed
                    config::fast_loop.beep_on(0.5);
                    state_ = START_VELOCITY;
                }
                if (torque_filtered > 5) {
                    state_ = ERROR;
                    config::fast_loop.beep_on(2);
                }
                break;
            case START_VELOCITY:
                command.mode_desired = VELOCITY;
                command.velocity_desired = 200;
                if (status_.torque > 4) {
                    config::fast_loop.beep_on(1);
                    state_ = TORQUE;
                }
                if (config::gpio1.is_set()) {
                    state_ = ERROR;
                    config::fast_loop.beep_on(2);
                }
                break;
            case TORQUE:
                command.mode_desired = MotorMode::TORQUE;
                if (velocity_filtered > 0) {
                    command.torque_desired = 5;
                } else {
                    command.torque_desired = 5;
                }
                if (config::gpio1.is_set() || config::gpio2.is_set()) {
                    // done
                    command.mode_desired = CURRENT; // necessary to get beep
                    config::main_loop.set_command(command);
                    asm("NOP");
                    config::fast_loop.beep_on(.3);
                    asm("NOP");
                    ms_delay(500);
                    asm("NOP");
                    config::fast_loop.beep_on(.3);
                    asm("NOP");
                    ms_delay(500);
                    asm("NOP");
                    config::fast_loop.beep_on(.3);
                    asm("NOP");
                    state_ = OFF;
                }
                break;
            case ERROR:
            case OFF:
            default:
                command.mode_desired = CURRENT; // necessary to get beep
                break;
        }

        config::main_loop.set_command(command);
    }
 private:
    uint64_t t_ms_ = 0;
    MainLoopStatus status_;
    bool command_received_ = false;
    float last_motor_position_ = 0;
    float last_timestamp_ = 0;
    FirstOrderLowPassFilter velocity_filter_ = {.001, 100};
    FirstOrderLowPassFilter torque_filter_ = {.001, 100};
    State state_ = LOW_VELOCITY;
};