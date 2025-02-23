#include "beo4_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.beo4";

// beo4 pulse width, high=carrier_pulse low=data_pulse
constexpr uint16_t PW_CARR_US = 200;     // carrier pulse length
constexpr uint16_t PW_ZERO_US = 2925;    // + 200 =  3125 µs
constexpr uint16_t PW_SAME_US = 6050;    // + 200 =  6250 µs
constexpr uint16_t PW_ONE_US = 9175;     // + 200 =  9375 µs
constexpr uint16_t PW_STOP_US = 12300;   // + 200 = 12500 µs
constexpr uint16_t PW_START_US = 15425;  // + 200 = 15625 µs

// beo4 pulse codes
constexpr uint8_t PC_ZERO = (PW_CARR_US + PW_ZERO_US) / 3125;    // =1
constexpr uint8_t PC_SAME = (PW_CARR_US + PW_SAME_US) / 3125;    // =2
constexpr uint8_t PC_ONE = (PW_CARR_US + PW_ONE_US) / 3125;      // =3
constexpr uint8_t PC_STOP = (PW_CARR_US + PW_STOP_US) / 3125;    // =4
constexpr uint8_t PC_START = (PW_CARR_US + PW_START_US) / 3125;  // =5

// beo4 number of data bits = beoLink+beoSrc+beoCmd = 1+8+8 = 17
constexpr uint32_t N_BITS = 1 + 8 + 8;

// required symbols = 2*(start_sequence + n_bits + stop) = 2*(3+17+1) = 42
constexpr uint32_t N_SYM = 2 + ((3 + 17 + 1) * 2u);  // + 2 = 44

// states finite-state-machine decoder
enum class rxSt { Idle, Data, Stop };

void Beo4Protocol::encode(RemoteTransmitData *dst, const Beo4Data &data) {
  uint32_t beoCode = ((uint32_t) data.source << 8) + (uint32_t) data.command;
  uint32_t jc = 0, ic = 0;  // loop counters
  uint32_t curBit = 0;      // current bit
  uint32_t preBit = 0;      // previous bit to compare
  dst->set_carrier_frequency(455000);
  dst->reserve(N_SYM);

  // start sequence=zero,zero,start
  dst->item(PW_CARR_US, PW_ZERO_US);
  dst->item(PW_CARR_US, PW_ZERO_US);
  dst->item(PW_CARR_US, PW_START_US);

  // the data-bit BeoLink is always 0
  dst->item(PW_CARR_US, PW_ZERO_US);

  // The B&O trick to avoid extra long and extra short
  // code-frames by extracting the data-bits from left
  // to right, then comparing current with previous bit
  // and set pulse to "same" "one" or "zero"
  for (jc = 15, ic = 0; ic < 16; ic++, jc--) {
    curBit = ((beoCode) >> jc) & 1;
    if (curBit == preBit)
      dst->item(PW_CARR_US, PW_SAME_US);
    else if (1 == curBit)
      dst->item(PW_CARR_US, PW_ONE_US);
    else
      dst->item(PW_CARR_US, PW_ZERO_US);
    preBit = curBit;
  }
  // complete the frame with stop-symbol and final carrier pulse
  dst->item(PW_CARR_US, PW_STOP_US);
  dst->mark(PW_CARR_US);
}

optional<Beo4Data> Beo4Protocol::decode(RemoteReceiveData src) {
  int32_t n_sym = src.size();  // number of recorded symbols
  Beo4Data data{
      // preset output data
      .source = 0,
      .command = 0,
      .repeats = 0,
  };
  if (n_sym > 42) {               // suppress dummy codes (TSO7000 hiccups)
    static uint32_t beoCode = 0;  // decoded beoCode
    rxSt rxFSM = rxSt::Idle;      // begin in idle state
    int32_t ic = 0, jc = 0;
    uint32_t preBit = 0;  // previous bit
    uint32_t cntBit = 0;  // bit counter
    ESP_LOGD(TAG, "Beo4: n_sym=%d ", n_sym);
    for (jc = 0, ic = 0; ic < (n_sym - 1); ic += 2, jc++) {
      int32_t pulseWidth = src[ic] - src[ic + 1];
      if (pulseWidth > 1500) {  // suppress TSOP7000 (dummy pulses)
        int32_t pulseCode = (pulseWidth + 1560) / 3125;
        switch (rxFSM) {
          case rxSt::Idle: {  // waiting until start-code
            beoCode = 0;
            cntBit = 0;
            preBit = 0;
            if (PC_START == pulseCode) {
              rxFSM = rxSt::Data;  // next--> collect data
            }
            break;
          }
          case rxSt::Data: {  // collecting data
            uint32_t curBit = 0;
            switch (pulseCode) {
              case PC_ZERO: {
                curBit = preBit = 0;
                break;
              }
              case PC_SAME: {
                curBit = preBit;
                break;
              }
              case PC_ONE: {
                curBit = preBit = 1;
                break;
              }
              default: {
                rxFSM = rxSt::Idle;  // frame is faulty, reset and..
                break;               // ..process further symbols
              }
            }
            beoCode = (beoCode << 1) + curBit;
            if (++cntBit == N_BITS) {  // beoCode is complete
              rxFSM = rxSt::Stop;      // next--> validate stop-code
            }
            break;
          }
          case rxSt::Stop: {             // validate stop code
            if (PC_STOP == pulseCode) {  // stop code valid
              data.source = (uint8_t) ((beoCode >> 8) & 0xff);
              data.command = (uint8_t) ((beoCode) &0xff);
              data.repeats++;  // update counter
            }
            if ((n_sym - ic) < 42) {
              return data;  // no more frames, so return here
            } else {
              rxFSM = rxSt::Idle;  // process further symbols
            }
            break;
          }
        }
      }
    }
  }
  return {};  // decoding failed
}

void Beo4Protocol::dump(const Beo4Data &data) {
  ESP_LOGI(TAG, "Beo4: source=0x%02x command=0x%02x repeats=%d ", data.source, data.command, data.repeats);
}

}  // namespace remote_base
}  // namespace esphome
