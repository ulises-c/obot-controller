#include "../param/param_obot_g474.h"
#include "st_device.h"
#include "../../motorlib/peripheral/stm32g4/spi_dma.h"
#include "../../motorlib/aksim2_encoder.h"
#include "../../motorlib/torque_sensor.h"
#include "../../motorlib/gpio.h"
#include "../../motorlib/bmi270.h"
#include "../../motorlib/peripheral/stm32g4/pin_config.h"
#define COMMS   COMMS_USB

using TorqueSensor = TorqueSensorBase;
//using MotorEncoder = Aksim2Encoder<18>;
using MotorEncoder = EncoderBase;
//using OutputEncoder = Aksim2Encoder<18>;
using OutputEncoder = EncoderBase;

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

      GPIO_SETL(C, 4, GPIO::OUTPUT, GPIO_SPEED::LOW, 0);        // imu cs
      GPIOC->BSRR |= GPIO_BSRR_BS4; // set imu cs

      GPIO_SETL(A, 0, GPIO_MODE::OUTPUT, GPIO_SPEED::LOW, 0);    // motor cs

      // imu
      SPI1->CR1 = SPI_CR1_MSTR | (4 << SPI_CR1_BR_Pos) | SPI_CR1_SSI | SPI_CR1_SSM;    // baud = clock/32
    }
};

namespace config {
    const uint32_t main_loop_frequency = 10000;
    const uint32_t pwm_frequency = 20000;
    InitCode init_code;

    GPIO output_encoder_cs(*GPIOC, 3, GPIO::OUTPUT);
    SPIDMA spi1_dma(SPIDMA::SP1, output_encoder_cs, DMA1_CH3, DMA1_CH4, 0);
    //OutputEncoder output_encoder(spi1_dma);
    EncoderBase motor_encoder;
    TorqueSensor torque_sensor;
    GPIO motor_encoder_cs(*GPIOA, 0, GPIO::OUTPUT);
    SPIDMA spi3_dma(SPIDMA::SP3, motor_encoder_cs, DMA1_CH1, DMA1_CH2, 0);
    //MotorEncoder motor_encoder(spi3_dma);


    // moved to ...motor.cpp
    // GPIO imu_cs(*GPIOC, 4, GPIO::OUTPUT);
    // SPIDMA spi1_dma2(SPIDMA::SP1, imu_cs, DMA1_CH3, DMA1_CH4, 0, 40, 40);
    // BMI270 imu(spi1_dma2);
    EncoderBase output_encoder;
};

#define SPI1_REINIT_CALLBACK
void spi1_reinit_callback() {
    SPI1->CR1=0;
    SPI1->CR2 = (7 << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH;   // 8 bit
    // ORDER DEPENDANCE SPE set last
    SPI1->CR1 = SPI_CR1_MSTR | (4 << SPI_CR1_BR_Pos) | SPI_CR1_SSI | SPI_CR1_SSM;    // baud = clock/64
    //config::spi1_dma2.reinit();
}

#include "../../motorlib/boards/config_obot_g474_motor.cpp"

void config_init() {

    System::api.add_api_variable("imu", new const APICallback([](){ return config::imu.get_string(); }));
    System::api.add_api_variable("ax", new const APICallbackFloat([](){ return config::imu.data_.acc_x*8./pow(2,15); }));
    System::api.add_api_variable("ay", new const APICallbackFloat([](){ return config::imu.data_.acc_y*8./pow(2,15); }));
    System::api.add_api_variable("az", new const APICallbackFloat([](){ return config::imu.data_.acc_z*8./pow(2,15); }));
    System::api.add_api_variable("gx", new const APICallbackFloat([](){ return config::imu.data_.gyr_x*2000.*M_PI/180/pow(2,15); }));
    System::api.add_api_variable("gy", new const APICallbackFloat([](){ return config::imu.data_.gyr_y*2000.*M_PI/180/pow(2,15); }));
    System::api.add_api_variable("gz", new const APICallbackFloat([](){ return config::imu.data_.gyr_z*2000.*M_PI/180/pow(2,15); }));


    GPIO_SETL(A, 4, 1, 0, 0);
    GPIOA->BSRR |= GPIO_BSRR_BS4; // set drv cs

    config::imu.init();
}

FrequencyLimiter imu_rate = {100};

void config_maintenance() {
    if (imu_rate.run()) {
        
        config::imu.read();
    }
}