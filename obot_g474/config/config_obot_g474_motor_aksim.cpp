#include "../../motorlib/boards/config_obot_g474_motor.h"
#include "../param/param_obot_g474_aksim.h"
#include "st_device.h"
#include "../../motorlib/peripheral/stm32g4/spi_dma.h"
#include "../../motorlib/aksim2_encoder.h"
#include "../../motorlib/torque_sensor.h"
#include "../../motorlib/gpio.h"
#include "../../motorlib/temperature_sensor.h"
//#include "../../motorlib/qia128.h"
#include "../../motorlib/peripheral/stm32g4/qia128_uart.h"
#include "../../motorlib/peripheral/stm32g4/pin_config.h"
//#include "../../motorlib/sensor_multiplex.h"
#include "../../motorlib/peripheral/stm32g4/max31889.h"
#include "../../motorlib/sensor_multiplex.h"
#define COMMS   COMMS_USB

const char obot_hash[41] __attribute__((section("flash_start"),used)) = OBOT_HASH;

#define END_TRIGGER_MOTOR_ENCODER

using MotorEncoder = Aksim2Encoder;

#ifdef JOINT_ENCODER_BITS
    #define CUSTOM_SENDDATA
    using OutputEncoder1 = SensorMultiplex<Aksim2Encoder, Aksim2Encoder>;
    using JointEncoder = OutputEncoder1::SecondarySensor;
#else
    using OutputEncoder1 = Aksim2Encoder;
#endif

#ifdef ADS8339_TORQUE_SENSOR
    #define INTERFACE_BBS
    #include "../../motorlib/ads8339.h"
    using TorqueSensor = TorqueSensorMultiplex<ADS8339, OutputEncoder1>;
    using OutputEncoder = TorqueSensor::SecondarySensor;
#elif defined(MAX11254_TORQUE_SENSOR)
    #define INTERFACE_BBL
    #define GPIO_OUT int gpio_out_1234
    #include "../../motorlib/sensors/torque/max11254.h"
    using TorqueSensor = TorqueSensorMultiplex<MAX11254<>, OutputEncoder1>;
    using OutputEncoder = TorqueSensor::SecondarySensor;
#else
    using TorqueSensor = QIA128_UART;
    using OutputEncoder = OutputEncoder1;
#endif



struct InitCode {
    InitCode() {
      SPI3->CR2 = (7 << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH;   // 8 bit
      // ORDER DEPENDANCE SPE set last
      SPI3->CR1 = SPI_CR1_MSTR | (5 << SPI_CR1_BR_Pos) | SPI_CR1_SSI | SPI_CR1_SSM | SPI_CR1_CPOL;    // baud = clock/64
           
      SPI1->CR2 = (7 << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH;   // 8 bit
      // ORDER DEPENDANCE SPE set last
      SPI1->CR1 = SPI_CR1_MSTR | (5 << SPI_CR1_BR_Pos) | SPI_CR1_SSI | SPI_CR1_SSM | SPI_CR1_CPOL;    // baud = clock/64
      DMAMUX1_Channel0->CCR =  DMA_REQUEST_SPI3_TX;
      DMAMUX1_Channel1->CCR =  DMA_REQUEST_SPI3_RX;
      DMAMUX1_Channel2->CCR =  DMA_REQUEST_SPI1_TX;
      DMAMUX1_Channel3->CCR =  DMA_REQUEST_SPI1_RX;

      
      // ORDER DEPENDANCE SPE set last
      SPI1->CR1 = SPI_CR1_MSTR | (6 << SPI_CR1_BR_Pos) | SPI_CR1_SSI | SPI_CR1_SSM;    // baud = clock/64
      // uart
    //   RCC->APB1ENR2 |= RCC_APB1ENR2_LPUART1EN;
    //   MASK_SET(RCC->CCIPR, RCC_CCIPR_LPUART1SEL, 1); // sysclk: 
    //   LPUART1->BRR = 256*CPU_FREQUENCY_HZ/320000;
    //   LPUART1->CR3 = 2 << USART_CR3_RXFTCFG_Pos; // 4 bytes fifo threshold
    //   LPUART1->CR1 = USART_CR1_FIFOEN | USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
    //   GPIO_SETL(C, 0, GPIO_MODE::ALT_FUN, GPIO_SPEED::LOW, 8);
    //   GPIO_SETL(C, 1, GPIO_MODE::ALT_FUN, GPIO_SPEED::LOW, 8);

#if !defined(MAX11254_TORQUE_SENSOR) && !defined(ADS8339_TORQUE_SENSOR)
        //uart5
        RCC->APB1ENR1 |= RCC_APB1ENR1_UART5EN;
        MASK_SET(RCC->CCIPR, RCC_CCIPR_UART5SEL, 1); // sysclk: 
        UART5->BRR = CPU_FREQUENCY_HZ/320000;
        UART5->CR3 = 2 << USART_CR3_RXFTCFG_Pos; // 4 bytes fifo threshold
        UART5->CR1 = USART_CR1_FIFOEN | USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
        GPIO_SETH(C, 12, GPIO_MODE::ALT_FUN, GPIO_SPEED::LOW, 5);
        GPIO_SETL(D, 2, GPIO_MODE::ALT_FUN, GPIO_SPEED::LOW, 5);
#endif

      GPIO_SETL(A, 0, GPIO_MODE::OUTPUT, GPIO_SPEED::VERY_HIGH, 0);   // PA0-> motor encoder cs
      //MASK_SET(GPIOC->PUPDR, GPIO_PUPDR_PUPD10, GPIO_PULL::UP);       // keep motor sclk high when spi disabled

      // gpio out
      GPIO_SETL(A, 1, GPIO::OUTPUT, GPIO_SPEED::VERY_HIGH, 0);
      // gpio in
      GPIO_SETL(A, 2, GPIO::INPUT, GPIO_SPEED::VERY_HIGH, 0);

#ifdef JOINT_ENCODER_BITS
      GPIO_SETL(C, 2, GPIO_MODE::OUTPUT, GPIO_SPEED::MEDIUM, 0); // A3 used as joint encoder cs
      GPIOC->BSRR = GPIO_BSRR_BS2;
#endif

      GPIO_SETH(A, 9, GPIO_MODE::OUTPUT, GPIO_SPEED::MEDIUM, 0);
#ifdef DISABLE_USBN_PULLDOWN
      GPIOA->BSRR = GPIO_BSRR_BR9;
#else
      GPIOA->BSRR = GPIO_BSRR_BS9;
#endif
      
#ifdef INTERFACE_BBL
      // PA0 TCS
      // PC12 TTCS (torque temperature CS)
      GPIO_SETL(A, 0, GPIO_MODE::OUTPUT, GPIO_SPEED::MEDIUM, 0); 
      GPIOA->BSRR = GPIO_BSRR_BS0;

      GPIO_SETH(C, 12, GPIO_MODE::OUTPUT, GPIO_SPEED::MEDIUM, 0); 
      GPIOC->BSRR = GPIO_BSRR_BS12;
#elif defined(INTERFACE_BBS)
      // PA0 TCS
      GPIO_SETL(A, 0, GPIO_MODE::OUTPUT, GPIO_SPEED::MEDIUM, 0); 
      GPIOA->BSRR = GPIO_BSRR_BS0;
#else
      GPIO_SETL(B, 3, GPIO_MODE::OUTPUT, GPIO_SPEED::MEDIUM, 0); // B3 (SWO), hdr6 torque sensor cs
      GPIOB->BSRR = GPIO_BSRR_BS3;

      GPIO_SETL(A, 1, GPIO_MODE::OUTPUT, GPIO_SPEED::MEDIUM, 0); // A1, hdr10 (QEPB) temp sensor cs (pz adapter board?)
      GPIOA->BSRR = GPIO_BSRR_BS1;
#endif

      GPIOC->BSRR = GPIO_BSRR_BS3;  // hdr17 (1CS2), output encoder cs
    }
};

namespace config {
    const uint32_t main_loop_frequency = 10000;
    const uint32_t pwm_frequency = 20000;
    InitCode init_code;

#if defined(INTERFACE_BBL) || defined(INTERFACE_BBS)
    GPIO motor_encoder_cs(*GPIOD, 2, GPIO::OUTPUT);
#else
    GPIO motor_encoder_cs(*GPIOA, 0, GPIO::OUTPUT);
#endif
    SPIDMA spi3_dma(SPIDMA::SP3, motor_encoder_cs, DMA1_CH1, DMA1_CH2, 0);
    MotorEncoder motor_encoder(spi3_dma, param->fast_loop_param.motor_encoder.cpr);
    
    GPIO output_encoder_cs(*GPIOC, 3, GPIO::OUTPUT);
    SPIDMA spi1_dma(SPIDMA::SP1, output_encoder_cs, DMA1_CH3, DMA1_CH4, 0, 100, 100,
        SPI_CR1_MSTR | (5 << SPI_CR1_BR_Pos) | SPI_CR1_SSI | SPI_CR1_SSM | SPI_CR1_CPOL);
    Aksim2Encoder output_encoder_direct(spi1_dma, param->main_loop_param.output_encoder.cpr);

#ifdef JOINT_ENCODER_BITS
    GPIO joint_encoder_cs(*GPIOC, 2, GPIO::OUTPUT);
    SPIDMA spi1_dma2(SPIDMA::SP1, joint_encoder_cs,  DMA1_CH3, DMA1_CH4, 100, 100, 
        SPI_CR1_MSTR | (5 << SPI_CR1_BR_Pos) | SPI_CR1_SSI | SPI_CR1_SSM | SPI_CR1_CPOL);
    Aksim2Encoder joint_encoder_direct(spi1_dma2, pow(2,18));
    OutputEncoder1 output_encoder1(output_encoder_direct, joint_encoder_direct);
    JointEncoder &joint_encoder = output_encoder1.secondary();
#else
    OutputEncoder1 &output_encoder1 = output_encoder_direct;
#endif // JOINT ENCODER

    

#if defined(INTERFACE_BBL) || defined(INTERFACE_BBS)
    GPIO torque_sensor_cs(*GPIOA, 0, GPIO::OUTPUT);
#else
    GPIO torque_sensor_cs(*GPIOB, 3, GPIO::OUTPUT);
#endif

#ifdef MAX11254_TORQUE_SENSOR
    SPIDMA spi1_dma3(SPIDMA::SP1, torque_sensor_cs, DMA1_CH3, DMA1_CH4, 0, 100, 100,
        SPI_CR1_MSTR | 6 << SPI_CR1_BR_Pos | SPI_CR1_SSI | SPI_CR1_SSM);
    MAX11254<> torque_sensor_direct(spi1_dma3, 1, false);
    TorqueSensor torque_sensor(torque_sensor_direct, output_encoder1);
    OutputEncoder &output_encoder = torque_sensor.secondary();
#elif defined(ADS8339_TORQUE_SENSOR)
    SPIDMA spi1_dma3(SPIDMA::SP1, torque_sensor_cs, DMA1_CH3, DMA1_CH4, 0, 100, 100,
        SPI_CR1_MSTR | 6 << SPI_CR1_BR_Pos | SPI_CR1_SSI | SPI_CR1_SSM);
    ADS8339 torque_sensor_direct(spi1_dma3, 0);
    TorqueSensor torque_sensor(torque_sensor_direct, output_encoder1);
    OutputEncoder &output_encoder = torque_sensor.secondary();
#else
    QIA128_UART torque_sensor(*UART5);
    OutputEncoder &output_encoder = output_encoder1;
#endif

};

#define SPI1_REINIT_CALLBACK
void spi1_reinit_callback() {
    // necessary to recover from sleep
    config::spi1_dma.reinit();
    uint32_t tmp = DMA1_Channel4->CCR;
    DMA1_Channel4->CCR = 0; // flush spi1 rx
    DMA1_Channel4->CNDTR = 0;
    DMA1_Channel4->CCR = tmp;
}

#include "../../motorlib/boards/config_obot_g474_motor.cpp"

namespace config {
    PT1000 motor_temperature(A1_DR, v3v3);
    MAX31889 ambient_temperature(i2c1);
    MAX31889 ambient_temperature_if(i2c1,1);
    MAX31889 ambient_temperature_3(i2c1,2);
    MAX31889 ambient_temperature_4(i2c1,3);
};

#ifdef JOINT_ENCODER_BITS
float joint_encoder_bias = 0;
bool joint_bias_set = false;
#endif


void config_init() {
    config::motor_pwm.set_frequency_multiplier(param->pwm_multiplier);
    System::api.add_api_variable("mdiag", new const APIUint8(&config::motor_encoder.diag_.word));
    System::api.add_api_variable("mdiag_raw", new const APIUint8(&config::motor_encoder.diag_raw_.word));
    System::api.add_api_variable("mcrc", new const APIUint8(&config::motor_encoder.crc_calc_));
    System::api.add_api_variable("merr", new APIUint32(&config::motor_encoder.diag_err_count_));
    System::api.add_api_variable("mwarn", new APIUint32(&config::motor_encoder.diag_warn_count_));
    System::api.add_api_variable("mcrc_cnt", new APIUint32(&config::motor_encoder.crc_err_count_));
    System::api.add_api_variable("mraw", new APIUint32(&config::motor_encoder.raw_value_));
    System::api.add_api_variable("mrawh", new const APICallback([](){ return u32_to_hex(config::motor_encoder.raw_value_); }));
    System::api.add_api_variable("mcrc_latch", new const APIUint32(&config::motor_encoder.crc_error_raw_latch_));
    System::api.add_api_variable("Tmotor", new const APICallbackFloat([](){ return config::motor_temperature.read(); }));
    System::api.add_api_variable("Tambient", new const APICallbackFloat([](){ return config::ambient_temperature.get_temperature(); }));
    System::api.add_api_variable("Tambient2", new const APICallbackFloat([](){ return config::ambient_temperature_if.get_temperature(); }));
    System::api.add_api_variable("Tambient3", new const APICallbackFloat([](){ return config::ambient_temperature_3.get_temperature(); }));
    System::api.add_api_variable("Tambient4", new const APICallbackFloat([](){ return config::ambient_temperature_4.get_temperature(); }));
#ifdef JOINT_ENCODER_BITS
    System::api.add_api_variable("jerr", new APIUint32(&config::joint_encoder_direct.diag_err_count_));
    System::api.add_api_variable("jwarn", new APIUint32(&config::joint_encoder_direct.diag_warn_count_));
    System::api.add_api_variable("jcrc_cnt", new APIUint32(&config::joint_encoder_direct.crc_err_count_));
    System::api.add_api_variable("jraw", new APIUint32(&config::joint_encoder_direct.raw_value_));
    System::api.add_api_variable("jrawh", new const APICallback([](){ return u32_to_hex(config::joint_encoder_direct.raw_value_); }));
    System::api.add_api_variable("jcrc_latch", new const APIUint32(&config::joint_encoder_direct.crc_error_raw_latch_));
    System::api.add_api_variable("jbias", new APIFloat(&joint_encoder_bias));
#endif
    System::api.add_api_variable("oerr", new APIUint32(&config::output_encoder_direct.diag_err_count_));
    System::api.add_api_variable("owarn", new APIUint32(&config::output_encoder_direct.diag_warn_count_));
    System::api.add_api_variable("ocrc_cnt", new APIUint32(&config::output_encoder_direct.crc_err_count_));
    System::api.add_api_variable("oraw", new APIUint32(&config::output_encoder_direct.raw_value_));
    System::api.add_api_variable("orawh", new const APICallback([](){ return u32_to_hex(config::output_encoder_direct.raw_value_); }));
    System::api.add_api_variable("ocrc_latch", new const APIUint32(&config::output_encoder_direct.crc_error_raw_latch_));

    System::api.add_api_variable("brr", new APIUint32(&LPUART1->BRR));
    System::api.add_api_variable("cr1", new APIUint32(&LPUART1->CR1));
    System::api.add_api_variable("isr", new APIUint32(&LPUART1->ISR));
#ifdef MAX11254_TORQUE_SENSOR
    System::api.add_api_variable("traw", new const APIUint32(&config::torque_sensor_direct.raw_value_));
    System::api.add_api_variable("tint", new const APIInt32(&config::torque_sensor_direct.signed_value_));
    System::api.add_api_variable("ttimeout_error", new const APIUint32(&config::torque_sensor_direct.timeout_error_));
    System::api.add_api_variable("tread_error", new const APIUint32(&config::torque_sensor_direct.read_error_));
    System::api.add_api_variable("tmux_delay", new APICallbackUint16([](){ return 0; }, [](uint16_t u){ config::torque_sensor_direct.write_reg16(5, u); }));
#elif defined(ADS8339_TORQUE_SENSOR)
    System::api.add_api_variable("traw", new const APIUint32(&config::torque_sensor_direct.raw_value_));
    System::api.add_api_variable("tint", new const APIInt32(&config::torque_sensor_direct.signed_value_));
    System::api.add_api_variable("ttimeout_error", new const APIUint32(&config::torque_sensor_direct.timeout_error_));
    System::api.add_api_variable("tread_error", new const APIUint32(&config::torque_sensor_direct.read_error_));
#else
    System::api.add_api_variable("traw", new const APIUint32(&config::torque_sensor.raw_));
    System::api.add_api_variable("twait_error", new const APIUint32(&config::torque_sensor.wait_error_));
    System::api.add_api_variable("tread_error", new const APIUint32(&config::torque_sensor.read_error_));
    System::api.add_api_variable("tread_len", new const APIUint32(&config::torque_sensor.read_len_));
    System::api.add_api_variable("tcrc_error", new const APIUint32(&config::torque_sensor.crc_error_));
    System::api.add_api_variable("tcrc_calc", new const APIUint8(&config::torque_sensor.crc_calc_));
    System::api.add_api_variable("tcrc_read", new const APIUint8(&config::torque_sensor.crc_read_));
    System::api.add_api_variable("twait_error", new const APIUint32(&config::torque_sensor.wait_error_));
    System::api.add_api_variable("ttimeout_error", new const APIUint32(&config::torque_sensor.timeout_error_));
    System::api.add_api_variable("tfull_raw", new const APIUint32(&config::torque_sensor.full_raw_));
    System::api.add_api_variable("qia_gain", new APICallbackUint8([](){ return config::torque_sensor.get_gain(); },
        [](uint8_t u){ config::torque_sensor.set_gain(u); }));
#endif
    // System::api.add_api_variable("5V", new const APIFloat(&v5v));
    // System::api.add_api_variable("V5V", new const APIUint32(&V5V));
    // System::api.add_api_variable("I5V", new const APIUint32(&I5V));
    // System::api.add_api_variable("i5V", new const APIFloat(&i5v));
    // System::api.add_api_variable("i48V", new const APIFloat(&i48v));
    // System::api.add_api_variable("IBUS", new const APIUint32(&I_BUS_DR));
    // System::api.add_api_variable("TSENSE", new const APIUint32(&TSENSE));
    // System::api.add_api_variable("TSENSE2", new const APIUint32(&TSENSE2));

    // watchdog reset
    IWDG->KR = 0xAAAA;
    ms_delay(200); // max aksim encoder startup time
    IWDG->KR = 0xAAAA;
}

MedianFilter<> motor_temperature_filter;
MedianFilter<> ambient_temperature_filter;
MedianFilter<> ambient2_temperature_filter;
MedianFilter<> ambient3_temperature_filter;
MedianFilter<> ambient4_temperature_filter;

FrequencyLimiter temp_rate_motor = {10};
void config_maintenance() {
    if(temp_rate_motor.run()) {
        float Tmotor = motor_temperature_filter.update(config::motor_temperature.read());
        config::main_loop.motor_temperature_model_.set_ambient_temperature(Tmotor);
        round_robin_logger.log_data(MOTOR_TEMPERATURE_INDEX, Tmotor);
        if (Tmotor > 120) {
            config::main_loop.status_.error.motor_temperature = true;
        }
        float Tambient = ambient_temperature_filter.update(config::ambient_temperature.read());
        round_robin_logger.log_data(AMBIENT_TEMPERATURE_INDEX, Tambient);
        float Tambient2 = ambient2_temperature_filter.update(config::ambient_temperature_if.read());
        round_robin_logger.log_data(AMBIENT_TEMPERATURE_2_INDEX, Tambient2);
        float Tambient3 = ambient3_temperature_filter.update(config::ambient_temperature_3.read());
        round_robin_logger.log_data(AMBIENT_TEMPERATURE_3_INDEX, Tambient3);
        float Tambient4 = ambient4_temperature_filter.update(config::ambient_temperature_4.read());
        round_robin_logger.log_data(AMBIENT_TEMPERATURE_4_INDEX, Tambient4);

    }
    if(config::motor_encoder.crc_err_count_ > 100 || config::motor_encoder.diag_err_count_ > 100 ||
        config::motor_encoder.diag_warn_count_ > pow(2,31)) {
            config::main_loop.status_.error.motor_encoder = true;
    }
    round_robin_logger.log_data(MOTOR_ENCODER_CRC_INDEX, config::motor_encoder.crc_err_count_);
    round_robin_logger.log_data(MOTOR_ENCODER_ERROR_INDEX, config::motor_encoder.diag_err_count_);
    round_robin_logger.log_data(MOTOR_ENCODER_WARNING_INDEX, config::motor_encoder.diag_warn_count_);
#ifdef NO_OCRC_FAULT
    uint32_t crc_fault_max =  pow(2,31);
#else
    uint32_t crc_fault_max = 100;
#endif
#ifdef JOINT_ENCODER_BITS
    if (!joint_bias_set) {
        if (config::joint_encoder_direct.get_value() != 0) {
            joint_bias_set = true;
            joint_encoder_bias = param->joint_encoder_bias;
            float joint_position = (float) config::joint_encoder_direct.get_value()*2*M_PI/pow(2,JOINT_ENCODER_BITS) + joint_encoder_bias;
            if (joint_position > param->joint_encoder_rollover) {
                joint_encoder_bias -= 2*M_PI;
            } else if (joint_position < -param->joint_encoder_rollover) {
                joint_encoder_bias += 2*M_PI;
            }
            logger.log_printf("joint encoder raw: %f, joint encoder bias: %f", (float) config::joint_encoder_direct.get_value()*2*M_PI/pow(2,JOINT_ENCODER_BITS), joint_encoder_bias);
        }
    }

    if(config::joint_encoder_direct.crc_err_count_ > crc_fault_max || config::joint_encoder_direct.diag_err_count_ > 100 ||
        config::joint_encoder_direct.diag_warn_count_ > pow(2,31)) {
            config::main_loop.status_.error.output_encoder = true;
    }
    round_robin_logger.log_data(JOINT_ENCODER_CRC_INDEX, config::joint_encoder_direct.crc_err_count_);
    round_robin_logger.log_data(JOINT_ENCODER_ERROR_INDEX, config::joint_encoder_direct.diag_err_count_);
    round_robin_logger.log_data(JOINT_ENCODER_WARNING_INDEX, config::joint_encoder_direct.diag_warn_count_);

    if (config::main_loop.mode_ == CLEAR_FAULTS) {
        config::joint_encoder_direct.clear_faults();
    }
#endif
    if(config::output_encoder_direct.crc_err_count_ > crc_fault_max || config::output_encoder_direct.diag_err_count_ > 100 ||
        config::output_encoder_direct.diag_warn_count_ > pow(2,31)) {
            config::main_loop.status_.error.output_encoder = true;
    }
    round_robin_logger.log_data(OUTPUT_ENCODER_CRC_INDEX, config::output_encoder_direct.crc_err_count_);
    round_robin_logger.log_data(OUTPUT_ENCODER_ERROR_INDEX, config::output_encoder_direct.diag_err_count_);
    round_robin_logger.log_data(OUTPUT_ENCODER_WARNING_INDEX, config::output_encoder_direct.diag_warn_count_);

#ifdef MAX11254_TORQUE_SENSOR
    round_robin_logger.log_data(TORQUE_SENSOR_ERROR_INDEX, config::torque_sensor_direct.timeout_error_ + config::torque_sensor_direct.read_error_);
    if (config::torque_sensor_direct.timeout_error_ > 10 ||
        config::torque_sensor_direct.read_error_ > 100) {
        config::main_loop.status_.error.torque_sensor = true;
    }
#elif defined(ADS8339_TORQUE_SENSOR)

#else
    round_robin_logger.log_data(TORQUE_SENSOR_CRC_INDEX, config::torque_sensor.crc_error_);
    round_robin_logger.log_data(TORQUE_SENSOR_ERROR_INDEX, config::torque_sensor.read_error_ + config::torque_sensor.wait_error_ + config::torque_sensor.timeout_error_);
    if (config::torque_sensor.timeout_error_ > 100) {
        config::main_loop.status_.error.torque_sensor = true;
    }
#endif
}

#ifdef JOINT_ENCODER_BITS
void load_send_data(const MainLoop &main_loop, SendData * const data) {
    static FirstOrderLowPassFilter joint_position_filter(1.0/config::main_loop_frequency, 1000);
    static FIRFilter<> joint_velocity_filter(1.0/config::main_loop_frequency, param->main_loop_param.output_filter_hz.output_velocity);
    data->iq = main_loop.status_.fast_loop.iq_filtered;
    data->host_timestamp_received = main_loop.host_timestamp_;
    data->mcu_timestamp = main_loop.status_.fast_loop.timestamp;
    data->motor_position = main_loop.status_.motor_position_filtered;
    data->reserved = main_loop.status_.output_position_filtered;
    data->motor_velocity = main_loop.status_.motor_velocity_filtered;
    
    data->torque = main_loop.status_.torque_filtered;
    float joint_position = (float) config::joint_encoder_direct.get_value()*2*M_PI/pow(2,JOINT_ENCODER_BITS) + joint_encoder_bias;
    data->joint_position = joint_position_filter.update(joint_position);
    data->joint_velocity = joint_velocity_filter.update(joint_position);
    data->torque = main_loop.status_.torque;
    data->rr_data = main_loop.status_.rr_data;
    data->reserved = main_loop.status_.output_position;
    data->iq_desired = main_loop.status_.fast_loop.foc_status.command.i_q;
    data->flags.mode = main_loop.status_.mode;
    data->flags.error = main_loop.status_.error;
    data->flags.misc.byte = 0;
#ifdef GPIO_IN
    data->flags.misc.gpio = GPIO_IN;
#endif  // GPIO_IN
}
#endif
