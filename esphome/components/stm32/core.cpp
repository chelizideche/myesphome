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

void init_uart() {}

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

void arch_init() {}

void IRAM_ATTR HOT arch_feed_wdt() {}

uint8_t progmem_read_byte(const uint8_t *addr) { return *addr; }

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

extern "C" {
void SysTick_Handler(void) { HAL_IncTick(); }

void Error_Handler(void) {
  while (1) {
  }
}
}

#endif  // USE_STM32
