//
// Created by khadiev on 17-12-2022
//
#include "alt_main.h"

GPIO_InitTypeDef GPIO_init_LED = {0};
TIM_HandleTypeDef TIM2_handler_init;
TIM_IC_InitTypeDef TIM_IC_user_init;
TIM_ClockConfigTypeDef TIM_clock_source_cfg;

enum IR_RECV_STATE {
    IR_IDLE,
    IR_MARK,
    IR_SPACE,
    IR_STOP,
};

typedef struct {
    uint32_t code; // here going to be IR code
    uint8_t code_length; // here going to be IR code length
    IR_RECV_STATE curr_state;
} IR_code;

uint32_t capture_data[256] = {0};
uint8_t capture_data_w = 0;
uint32_t capture_data_2[256] = {0};

IR_code IR_data[10];
volatile uint32_t IR_data_counter = 0;
#define START_US_HDR_1 930// 829 for AC_LG
#define START_US_HDR_2 450// 399 for AC_LG
#define MARK_US 63 // 55 AC_LG
#define SPACE_ZERO_US 55 // 45 for AC_LG
#define SPACE_ONE_US 162 // 148 for AC_LG

#define BETWEEN_START_US_HDR1(us) ( (bool ) (us > START_US_HDR_1 * 0.75  && us < START_US_HDR_1 * 1.25) )
#define BETWEEN_START_US_HDR2(us) ( (bool ) (us > START_US_HDR_2 * 0.75  && us < START_US_HDR_2 * 1.25) )
#define BETWEEN_MARK_US(us) ( (bool ) (us > MARK_US * 0.75  && us < MARK_US * 1.25) )
#define BETWEEN_SPACE_ZERO_US(us) ( (int ) (us > SPACE_ZERO_US * 0.75  && us < SPACE_ZERO_US * 1.25) )
#define BETWEEN_SPACE_ONE_US(us) ( (int ) (us > SPACE_ONE_US * 0.75  && us < SPACE_ONE_US * 1.25) )

int alt_main() {
    /* Initialization */
    init_GPIO();
    init_TIM2();
    IR_data[IR_data_counter].curr_state = IR_IDLE;

    HAL_TIM_Base_Start_IT(&TIM2_handler_init);
    HAL_TIM_IC_Start_IT(&TIM2_handler_init, TIM_CHANNEL_1);
    while (true) {
        /* Super loop */
    }
}

void init_GPIO() {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_init_LED.Pin       = GPIO_PIN_9 | GPIO_PIN_8| GPIO_PIN_7;
    GPIO_init_LED.Mode      = GPIO_MODE_OUTPUT_PP;
    GPIO_init_LED.Pull      = GPIO_NOPULL;
    GPIO_init_LED.Speed     = GPIO_SPEED_FREQ_MEDIUM;

    HAL_GPIO_Init(GPIOB, &GPIO_init_LED);
}

void init_TIM2() {
    TIM2_handler_init.Instance = TIM2;
    TIM2_handler_init.Init.Prescaler = 79;
    TIM2_handler_init.Init.CounterMode = TIM_COUNTERMODE_UP;
    TIM2_handler_init.Init.Period = 0xffff;
    TIM2_handler_init.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    TIM2_handler_init.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    HAL_TIM_Base_Init(&TIM2_handler_init);

    TIM_clock_source_cfg.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&TIM2_handler_init, &TIM_clock_source_cfg);

    HAL_TIM_IC_Init(&TIM2_handler_init);

    TIM_IC_user_init.ICFilter = 0;
    TIM_IC_user_init.ICPolarity = TIM_INPUTCHANNELPOLARITY_BOTHEDGE;
    TIM_IC_user_init.ICSelection = TIM_ICSELECTION_DIRECTTI;
    TIM_IC_user_init.ICPrescaler = TIM_ICPSC_DIV1;
    HAL_TIM_IC_ConfigChannel(&TIM2_handler_init, &TIM_IC_user_init, TIM_CHANNEL_1);

}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* tim_baseHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(tim_baseHandle->Instance==TIM2)
    {
        /* TIM2 clock enable */
        __HAL_RCC_TIM2_CLK_ENABLE();

        __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_0;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
        GPIO_InitStruct.Alternate = GPIO_AF2_TIM2;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* TIM2 interrupt Init */
        HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(TIM2_IRQn);
        //HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);

    }
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* tim_baseHandle)
{

    if(tim_baseHandle->Instance==TIM2)
    {
        /* Peripheral clock disable */
        __HAL_RCC_TIM2_CLK_DISABLE();

        /**TIM2 GPIO Configuration
        PA2     ------> TIM2_CH3
        */
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_0);

        /* TIM2 interrupt Deinit */
        HAL_NVIC_DisableIRQ(TIM2_IRQn);
    }
}

void TIM2_IRQHandler(void) {
    HAL_TIM_IRQHandler(&TIM2_handler_init);
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        capture_data[capture_data_w] = HAL_TIM_ReadCapturedValue(&TIM2_handler_init, TIM_CHANNEL_1);
        capture_data_2[capture_data_w] = capture_data[capture_data_w] - capture_data[capture_data_w - 1];

        capture_data_w++;
        if (decode()) {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
            for (volatile int i = 0; i < capture_data_w; i++) {
                capture_data_2[i] = 0;
                capture_data[i] = 0;
            }
            capture_data_w = 0;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
            HAL_NVIC_EnableIRQ(TIM2_IRQn);
            IR_data_counter++;
        }
    }
}

uint8_t decode() {
    if (capture_data_w < 72) return 0;
    volatile uint8_t i = 1;
    HAL_NVIC_DisableIRQ(TIM2_IRQn);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);

    for (; i < capture_data_w; i++) {
        switch (IR_data[IR_data_counter].curr_state) {
            case IR_IDLE: {
                if (i == 1 && BETWEEN_START_US_HDR1(capture_data_2[i])) {
                    i++;
                }
                if (i == 2 && BETWEEN_START_US_HDR2(capture_data_2[i])) {
                    IR_data[IR_data_counter].code = 0;
                    IR_data[IR_data_counter].code_length = 0;
                    IR_data[IR_data_counter].curr_state = IR_MARK;
                }
                break;
            }
            case IR_MARK: {
                if (BETWEEN_MARK_US(capture_data_2[i])) {
                    IR_data[IR_data_counter].curr_state = IR_SPACE;
                } else
                    IR_data[IR_data_counter].curr_state = IR_STOP;
                break;
            }
            case IR_SPACE: {
                if (BETWEEN_SPACE_ZERO_US(capture_data_2[i])) {
                    IR_data[IR_data_counter].code <<= 1;
                    IR_data[IR_data_counter].code_length++;
                    IR_data[IR_data_counter].curr_state = IR_MARK;
                } else if (BETWEEN_SPACE_ONE_US(capture_data_2[i])) {
                    IR_data[IR_data_counter].code <<= 1;
                    IR_data[IR_data_counter].code |= 0x1;
                    IR_data[IR_data_counter].code_length++;
                    IR_data[IR_data_counter].curr_state = IR_MARK;
                } else
                    IR_data[IR_data_counter].curr_state = IR_STOP;
                break;
            }
            case IR_STOP: {
                return 1;
            }
        }
    }
    return 1;
}
