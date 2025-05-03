#ifdef USE_STM32
#include "core.h"
#include "esphome/core/log.h"

static const char *TAG = "main";

namespace esphome {

namespace stm32 {
#if (__CORTEX_M >= 0x03)
static uint32_t prev_dwt_cycle_cnt = 0;
static uint64_t cycle_cnt = 0;

uint64_t get_dwt_cycle_cnt() {
  uint32_t dwt_cycle_cnt = DWT->CYCCNT;
  uint32_t delta = dwt_cycle_cnt - prev_dwt_cycle_cnt;
  cycle_cnt += delta;
  prev_dwt_cycle_cnt = dwt_cycle_cnt;
  return cycle_cnt;
}
#endif

void hal_init() {
  HAL_Init();
#if (__CORTEX_M >= 0x03)
  // Enable the DWT Cycle Counter for micros()/delayMicroseconds()
  if (!(CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk)) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  // Enable TRCENA
  }
  DWT->CYCCNT = 0;  // Reset cycle counter
  if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk)) {
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;  // Enable cycle counter
  }
#endif
}

static UART_HandleTypeDef UartHandle;

void init_uart() {
  GPIO_InitTypeDef GPIO_InitStruct;

  USARTx_TX_GPIO_CLK_ENABLE();
  USARTx_RX_GPIO_CLK_ENABLE();

  USARTx_CLK_ENABLE();

  GPIO_InitStruct.Pin = USARTx_TX_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
#ifdef GPIO_SPEED_FAST
  GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
#endif

  GPIO_InitStruct.Alternate = USARTx_TX_AF;

  HAL_GPIO_Init(USARTx_TX_GPIO_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = USARTx_RX_PIN;
  GPIO_InitStruct.Alternate = USARTx_RX_AF;

  HAL_GPIO_Init(USARTx_RX_GPIO_PORT, &GPIO_InitStruct);
  UartHandle.Instance = USARTx;

  UartHandle.Init.BaudRate = 115200;
  UartHandle.Init.WordLength = UART_WORDLENGTH_8B;
  UartHandle.Init.StopBits = UART_STOPBITS_1;
  UartHandle.Init.Parity = UART_PARITY_NONE;
  UartHandle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  UartHandle.Init.Mode = UART_MODE_TX_RX;
  UartHandle.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&UartHandle) != HAL_OK) {
    for (;;)
      ;
  }
}

void uart_write_char(char c) { HAL_UART_Transmit(&UartHandle, (uint8_t *) (&c), 1, 1000); }

void uart_write_str(const char *str) { HAL_UART_Transmit(&UartHandle, (uint8_t *) str, strlen(str), 1000); }

}  // namespace stm32

uint32_t random_uint32() { return 42; }

uint32_t IRAM_ATTR HOT millis() { return HAL_GetTick(); }

void IRAM_ATTR HOT delay(uint32_t ms) { HAL_Delay(ms); }

uint32_t IRAM_ATTR HOT micros() {
#if (__CORTEX_M >= 0x03)
  uint32_t hclk_freq = HAL_RCC_GetHCLKFreq();
  if (hclk_freq == 0)
    return 0;  // Clock not configured?
  return (uint32_t) ((stm32::get_dwt_cycle_cnt() * 1000000ULL) / hclk_freq);
#else
  return millis() * 1000;
#endif
}

void IRAM_ATTR HOT delayMicroseconds(uint32_t us) {
  if (us == 0)
    return;
#if (__CORTEX_M >= 0x03)
  // Ensure DWT cycle counter is enabled
  uint32_t hclk_freq = HAL_RCC_GetHCLKFreq();
  if (hclk_freq == 0)
    return;  // Clock not configured?

  uint64_t target_cycle_cnt = stm32::get_dwt_cycle_cnt() + (uint64_t) us * hclk_freq / 1000000ULL;

  while (target_cycle_cnt > stm32::get_dwt_cycle_cnt()) {
    __NOP();  // Ensure loop doesn't get optimized away
  }
#else
  delay(us / 1000);
#endif
}

void IRAM_ATTR HOT yield() {}

void arch_restart() {
  HAL_NVIC_SystemReset();
  // This function should not return. Loop forever if it does.
  while (1) {
  }
}

void arch_init() { ::esphome::stm32::init_uart(); }

void IRAM_ATTR HOT arch_feed_wdt() {}

uint8_t progmem_read_byte(const uint8_t *addr) {
  return *addr;
}

uint32_t IRAM_ATTR HOT arch_get_cpu_cycle_count() {
#if (__CORTEX_M >= 0x03)
  return DWT->CYCCNT;
#else
  return 0;  // Not available or easily accessible on M0/M0+ without custom timer
#endif
}

uint32_t arch_get_cpu_freq_hz() { return HAL_RCC_GetHCLKFreq(); }

}  // namespace esphome

int main() {
  ::esphome::stm32::hal_init();

  setup();

  while (1) {
    loop();
  }
}

#endif  // USE_STM32