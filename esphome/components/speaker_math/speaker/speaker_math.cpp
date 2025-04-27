#include "speaker_math.h"

#ifdef USE_ESP32

#include "esphome/components/audio/audio_transfer_buffer.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

namespace esphome {
namespace speaker_math {

static const UBaseType_t SPEAKER_MATH_TASK_PRIORITY = 1;

static const uint32_t TRANSFER_BUFFER_DURATION_MS = 50;

static const uint32_t TASK_DELAY_MS = 20;
static const uint32_t TASK_STACK_SIZE = 3072;

static const char *const TAG = "speaker_math";

enum SpeakerMathEventGroupBits : uint32_t {
  COMMAND_STOP = (1 << 0),  // stops the speaker math task
  STATE_STARTING = (1 << 10),
  STATE_RUNNING = (1 << 11),
  STATE_STOPPING = (1 << 12),
  STATE_STOPPED = (1 << 13),
  ERR_ESP_NO_MEM = (1 << 19),
  ERR_ESP_NOT_SUPPORTED = (1 << 20),
  ERR_ESP_FAIL = (1 << 21),
  ALL_BITS = 0x00FFFFFF,  // All valid FreeRTOS event group bits
};

void SpeakerMath::setup() {
  this->event_group_ = xEventGroupCreate();

  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create event group");
    this->mark_failed();
    return;
  }

  // pass callbacks along
  this->output_speaker_->add_audio_output_callback(
      [this](uint32_t new_playback_ms, uint32_t remainder_us, uint32_t pending_ms, uint32_t write_timestamp) {
        this->audio_output_callback_(new_playback_ms, remainder_us, pending_ms, write_timestamp);
      });
}

void SpeakerMath::loop() {
  uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

  if (event_group_bits & SpeakerMathEventGroupBits::STATE_STARTING) {
    ESP_LOGD(TAG, "Starting speaker_math task");
    xEventGroupClearBits(this->event_group_, SpeakerMathEventGroupBits::STATE_STARTING);
  }

  if (event_group_bits & SpeakerMathEventGroupBits::ERR_ESP_NO_MEM) {
    this->status_set_error("Speaker math task failed to allocate the internal buffers");
    xEventGroupClearBits(this->event_group_, SpeakerMathEventGroupBits::ERR_ESP_NO_MEM);
    this->state_ = speaker::STATE_STOPPING;
  }
  if (event_group_bits & SpeakerMathEventGroupBits::ERR_ESP_NOT_SUPPORTED) {
    this->status_set_error("Cannot apply math due to an unsupported audio stream");
    xEventGroupClearBits(this->event_group_, SpeakerMathEventGroupBits::ERR_ESP_NOT_SUPPORTED);
    this->state_ = speaker::STATE_STOPPING;
  }
  if (event_group_bits & SpeakerMathEventGroupBits::ERR_ESP_FAIL) {
    this->status_set_error("Speaker math task failed");
    xEventGroupClearBits(this->event_group_, SpeakerMathEventGroupBits::ERR_ESP_FAIL);
    this->state_ = speaker::STATE_STOPPING;
  }

  if (event_group_bits & SpeakerMathEventGroupBits::STATE_RUNNING) {
    ESP_LOGD(TAG, "Started speaker math task");
    this->status_clear_error();
    xEventGroupClearBits(this->event_group_, SpeakerMathEventGroupBits::STATE_RUNNING);
  }
  if (event_group_bits & SpeakerMathEventGroupBits::STATE_STOPPING) {
    ESP_LOGD(TAG, "Stopping speaker math task");
    xEventGroupClearBits(this->event_group_, SpeakerMathEventGroupBits::STATE_STOPPING);
  }
  if (event_group_bits & SpeakerMathEventGroupBits::STATE_STOPPED) {
    if (this->delete_task_() == ESP_OK) {
      ESP_LOGD(TAG, "Stopped speaker math task");
      xEventGroupClearBits(this->event_group_, SpeakerMathEventGroupBits::ALL_BITS);
    }
  }

  switch (this->state_) {
    case speaker::STATE_STARTING: {
      esp_err_t err = this->start_();
      if (err == ESP_OK) {
        this->status_clear_error();
        this->state_ = speaker::STATE_RUNNING;
      } else {
        switch (err) {
          case ESP_ERR_INVALID_STATE:
            this->status_set_error("Failed to start converter: speaker math task failed to start");
            break;
          case ESP_ERR_NO_MEM:
            this->status_set_error("Failed to start converter: not enough memory for task stack");
          default:
            this->status_set_error("Failed to start converter");
            break;
        }

        this->state_ = speaker::STATE_STOPPING;
      }
      break;
    }
    case speaker::STATE_RUNNING:
      if (this->output_speaker_->is_stopped()) {
        this->state_ = speaker::STATE_STOPPING;
      }

      break;
    case speaker::STATE_STOPPING:
      this->stop_();
      this->state_ = speaker::STATE_STOPPED;
      break;
    case speaker::STATE_STOPPED:
      break;
  }
}

size_t SpeakerMath::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  if (this->is_stopped()) {
    this->start();
  }

  size_t bytes_written = 0;

  if (this->ring_buffer_.use_count() == 1) {
    std::shared_ptr<RingBuffer> temp_ring_buffer = this->ring_buffer_.lock();
    bytes_written = temp_ring_buffer->write_without_replacement(data, length, ticks_to_wait);
  }

  return bytes_written;
}

void SpeakerMath::start() { this->state_ = speaker::STATE_STARTING; }

esp_err_t SpeakerMath::start_() {
  this->target_stream_info_ =
      audio::AudioStreamInfo(this->target_bits_per_sample_, this->audio_stream_info_.get_channels(),
                             this->audio_stream_info_.get_sample_rate());

  this->output_speaker_->set_audio_stream_info(this->target_stream_info_);
  this->output_speaker_->start();

  // Start the speaker math task to handle converting data
  return this->start_task_();

  return ESP_OK;
}

esp_err_t SpeakerMath::start_task_() {
  if (this->task_stack_buffer_ == nullptr) {
    if (this->task_stack_in_psram_) {
      RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
      this->task_stack_buffer_ = stack_allocator.allocate(TASK_STACK_SIZE);
    } else {
      RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
      this->task_stack_buffer_ = stack_allocator.allocate(TASK_STACK_SIZE);
    }
  }

  if (this->task_stack_buffer_ == nullptr) {
    return ESP_ERR_NO_MEM;
  }

  if (this->task_handle_ == nullptr) {
    this->task_handle_ = xTaskCreateStatic(convert_task, "sample", TASK_STACK_SIZE, (void *) this,
                                           SPEAKER_MATH_TASK_PRIORITY, this->task_stack_buffer_, &this->task_stack_);
  }

  if (this->task_handle_ == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }

  return ESP_OK;
}

void SpeakerMath::stop() { this->state_ = speaker::STATE_STOPPING; }

void SpeakerMath::stop_() {
  if (this->task_handle_ != nullptr) {
    xEventGroupSetBits(this->event_group_, SpeakerMathEventGroupBits::COMMAND_STOP);
  }
  this->output_speaker_->stop();
}

esp_err_t SpeakerMath::delete_task_() {
  if (!this->task_created_) {
    this->task_handle_ = nullptr;

    if (this->task_stack_buffer_ != nullptr) {
      if (this->task_stack_in_psram_) {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
        stack_allocator.deallocate(this->task_stack_buffer_, TASK_STACK_SIZE);
      } else {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
        stack_allocator.deallocate(this->task_stack_buffer_, TASK_STACK_SIZE);
      }

      this->task_stack_buffer_ = nullptr;
    }

    return ESP_OK;
  }

  return ESP_ERR_INVALID_STATE;
}

void SpeakerMath::finish() { this->output_speaker_->finish(); }

bool SpeakerMath::has_buffered_data() const {
  bool has_ring_buffer_data = false;
  return (has_ring_buffer_data || this->output_speaker_->has_buffered_data());
}

void SpeakerMath::set_mute_state(bool mute_state) {
  this->mute_state_ = mute_state;
  this->output_speaker_->set_mute_state(mute_state);
}

void SpeakerMath::set_volume(float volume) {
  this->volume_ = volume;
  this->output_speaker_->set_volume(volume);
}

void SpeakerMath::convert_task(void *params) {
  SpeakerMath *this_speaker_math = (SpeakerMath *) params;

  this_speaker_math->task_created_ = true;
  xEventGroupSetBits(this_speaker_math->event_group_, SpeakerMathEventGroupBits::STATE_STARTING);

  esp_err_t err = ESP_OK;

  std::shared_ptr<RingBuffer> temp_ring_buffer =
      RingBuffer::create(this_speaker_math->audio_stream_info_.ms_to_bytes(this_speaker_math->buffer_duration_ms_));

  // size an output buffer that's somewhat smaller than the buffer that passes data to the task
  size_t output_buffer_size =
      this_speaker_math->audio_stream_info_.ms_to_bytes(this_speaker_math->buffer_duration_ms_) / 2;

  // buffer which will hold data while we process it
  auto output_buffer = audio::AudioSinkTransferBuffer::create(output_buffer_size);
  output_buffer->set_sink(this_speaker_math->output_speaker_);

  if (temp_ring_buffer.use_count() == 0 || output_buffer == nullptr) {
    err = ESP_ERR_NO_MEM;
  } else {
    this_speaker_math->ring_buffer_ = temp_ring_buffer;

    this_speaker_math->output_speaker_->set_audio_stream_info(this_speaker_math->target_stream_info_);
  }

  if (err == ESP_OK) {
    xEventGroupSetBits(this_speaker_math->event_group_, SpeakerMathEventGroupBits::STATE_RUNNING);
  } else if (err == ESP_ERR_NO_MEM) {
    xEventGroupSetBits(this_speaker_math->event_group_, SpeakerMathEventGroupBits::ERR_ESP_NO_MEM);
  } else if (err == ESP_ERR_NOT_SUPPORTED) {
    xEventGroupSetBits(this_speaker_math->event_group_, SpeakerMathEventGroupBits::ERR_ESP_NOT_SUPPORTED);
  }

  // make local const copies of data to reduce pointer dereferences in inner loops
  const auto convert_unsigned = this_speaker_math->convert_unsigned_;
  const auto convert_factor = this_speaker_math->convert_factor_;
  const auto convert_offset = this_speaker_math->convert_offset_;
  const auto bits_per_sample = this_speaker_math->audio_stream_info_.get_bits_per_sample();

  // The core operation of the loop varies with the target data type, and so to
  // avoid a mess of repetition, we define the loop core based on datatype
  // We sacrifice compile time and (to a much lesser extent with optimizations enabled)
  // code size in order to gain improved performance (much fewer conditionals)
#define SPEAKER_MATH_LOOP_CORE(DATATYPE) \
  while (err == ESP_OK) { \
    uint32_t event_bits = xEventGroupGetBits(this_speaker_math->event_group_); \
\
    if (event_bits & SpeakerMathEventGroupBits::COMMAND_STOP) { \
      break; \
    } \
    if (output_buffer->available() > 0) { \
      ESP_LOGE(TAG, "Conversion buffer did not empty"); \
      xEventGroupSetBits(this_speaker_math->event_group_, SpeakerMathEventGroupBits::ERR_ESP_FAIL); \
      break; \
    } \
    /* read data from ring into processing buffer */ \
    auto bytes_read = temp_ring_buffer->read(output_buffer->get_buffer_end(), output_buffer->free()); \
    output_buffer->increase_buffer_length(bytes_read); \
    DATATYPE *convert_buffer = (DATATYPE *) output_buffer->get_buffer_start(); \
    const auto available_elements = output_buffer->available() / sizeof(DATATYPE); \
    ESP_LOGV(TAG, "Got %zd elements to do math on", available_elements); \
\
    for (int i = 0; i < available_elements; i++) { \
      /*we already cast into the unsigned data type, but we still need to actually convert it  to the new range*/ \
      if (convert_unsigned) { \
        convert_buffer[i] ^= (1ULL << (sizeof(DATATYPE) - 1)); \
      } \
      convert_buffer[i] += convert_offset; \
      convert_buffer[i] += convert_factor; \
    } \
    /* code will go boom if we don't empty the whole darn buffer*/ \
    output_buffer->transfer_data_to_sink(1000); \
  }

// In most cases, one or both of the offset/factor will be the identity.  A check with the
// compiler explorer shows that wrapping the loop in a if() construct to check for these
// cases will result in it generating optimized code for these cases
// We gate this behind a check if optimization is enabled, because otherwise this will
// make REALLY big code for no reason
#ifdef __OPTIMIZE__
#define SPEAKER_MATH_LOOP(DATATYPE) \
  { \
    if (convert_factor == 1 && convert_offset == 0) { \
      SPEAKER_MATH_LOOP_CORE(DATATYPE) \
    } \
\
    else if (convert_offset == 0) { \
      SPEAKER_MATH_LOOP_CORE(DATATYPE) \
    } \
\
    else if (convert_factor == 1) { \
      SPEAKER_MATH_LOOP_CORE(DATATYPE) \
    } \
\
    else { \
      SPEAKER_MATH_LOOP_CORE(DATATYPE) \
    } \
  }
#else
#define SPEAKER_MATH_LOOP(DATATYPE) \
  { SPEAKER_MATH_LOOP_CORE(DATATYPE) }
#endif

  ESP_LOGD(TAG, "Starting speaker math task with bits per sample %" PRIu8, bits_per_sample);

  if (bits_per_sample == 8 && convert_unsigned)
    SPEAKER_MATH_LOOP_CORE(uint8_t)
  if (bits_per_sample == 8 && !convert_unsigned)
    SPEAKER_MATH_LOOP_CORE(int8_t)
  if (bits_per_sample == 16 && convert_unsigned)
    SPEAKER_MATH_LOOP_CORE(uint16_t)
  if (bits_per_sample == 16 && !convert_unsigned)
    SPEAKER_MATH_LOOP_CORE(int16_t)
  if (bits_per_sample == 32 && convert_unsigned)
    SPEAKER_MATH_LOOP_CORE(uint32_t)
  if (bits_per_sample == 32 && !convert_unsigned)
    SPEAKER_MATH_LOOP_CORE(int32_t)

  xEventGroupSetBits(this_speaker_math->event_group_, SpeakerMathEventGroupBits::STATE_STOPPING);
  xEventGroupSetBits(this_speaker_math->event_group_, SpeakerMathEventGroupBits::STATE_STOPPED);
  this_speaker_math->task_created_ = false;
  vTaskDelete(nullptr);
}

}  // namespace speaker_math
}  // namespace esphome

#endif
