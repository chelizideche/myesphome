#include "api_connection.h"
#ifdef USE_API
#include <cerrno>
#include <cinttypes>
#include <utility>
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"

#ifdef USE_DEEP_SLEEP
#include "esphome/components/deep_sleep/deep_sleep_component.h"
#endif
#ifdef USE_HOMEASSISTANT_TIME
#include "esphome/components/homeassistant/time/homeassistant_time.h"
#endif
#ifdef USE_BLUETOOTH_PROXY
#include "esphome/components/bluetooth_proxy/bluetooth_proxy.h"
#endif
#ifdef USE_VOICE_ASSISTANT
#include "esphome/components/voice_assistant/voice_assistant.h"
#endif

namespace esphome {
namespace api {

static const char *const TAG = "api.connection";
static const int ESP32_CAMERA_STOP_STREAM = 5000;

APIConnection::APIConnection(std::unique_ptr<socket::Socket> sock, APIServer *parent)
    : parent_(parent), initial_state_iterator_(this), list_entities_iterator_(this) {
  this->proto_write_buffer_.reserve(64);

#if defined(USE_API_PLAINTEXT) && defined(USE_API_NOISE)
  auto noise_ctx = parent->get_noise_ctx();
  if (noise_ctx->has_psk()) {
    this->helper_ = std::unique_ptr<APIFrameHelper>{new APINoiseFrameHelper(std::move(sock), noise_ctx)};
  } else {
    this->helper_ = std::unique_ptr<APIFrameHelper>{new APIPlaintextFrameHelper(std::move(sock))};
  }
#elif defined(USE_API_PLAINTEXT)
  this->helper_ = std::unique_ptr<APIFrameHelper>{new APIPlaintextFrameHelper(std::move(sock))};
#elif defined(USE_API_NOISE)
  this->helper_ = std::unique_ptr<APIFrameHelper>{new APINoiseFrameHelper(std::move(sock), parent->get_noise_ctx())};
#else
#error "No frame helper defined"
#endif
}
void APIConnection::start() {
  this->last_traffic_ = App.get_loop_component_start_time();

  // Set next_ping_retry_ to prevent immediate ping
  // This ensures the first ping happens after the keepalive period
  this->next_ping_retry_ = this->last_traffic_ + KEEPALIVE_TIMEOUT_MS;

  APIError err = this->helper_->init();
  if (err != APIError::OK) {
    on_fatal_error();
    ESP_LOGW(TAG, "%s: Helper init failed: %s errno=%d", this->client_combined_info_.c_str(), api_error_to_str(err),
             errno);
    return;
  }
  this->client_info_ = helper_->getpeername();
  this->client_peername_ = this->client_info_;
  this->helper_->set_log_info(this->client_info_);
}

APIConnection::~APIConnection() {
#ifdef USE_BLUETOOTH_PROXY
  if (bluetooth_proxy::global_bluetooth_proxy->get_api_connection() == this) {
    bluetooth_proxy::global_bluetooth_proxy->unsubscribe_api_connection(this);
  }
#endif
#ifdef USE_VOICE_ASSISTANT
  if (voice_assistant::global_voice_assistant->get_api_connection() == this) {
    voice_assistant::global_voice_assistant->client_subscription(this, false);
  }
#endif
}

void APIConnection::loop() {
  if (this->remove_)
    return;

  if (!network::is_connected()) {
    // when network is disconnected force disconnect immediately
    // don't wait for timeout
    this->on_fatal_error();
    ESP_LOGW(TAG, "%s: Network unavailable; disconnecting", this->client_combined_info_.c_str());
    return;
  }
  if (this->next_close_) {
    // requested a disconnect
    this->helper_->close();
    this->remove_ = true;
    return;
  }

  APIError err = this->helper_->loop();
  if (err != APIError::OK) {
    on_fatal_error();
    ESP_LOGW(TAG, "%s: Socket operation failed: %s errno=%d", this->client_combined_info_.c_str(),
             api_error_to_str(err), errno);
    return;
  }

  // Check if socket has data ready before attempting to read
  if (this->helper_->is_socket_ready()) {
    ReadPacketBuffer buffer;
    err = this->helper_->read_packet(&buffer);
    if (err == APIError::WOULD_BLOCK) {
      // pass
    } else if (err != APIError::OK) {
      on_fatal_error();
      if (err == APIError::SOCKET_READ_FAILED && errno == ECONNRESET) {
        ESP_LOGW(TAG, "%s: Connection reset", this->client_combined_info_.c_str());
      } else if (err == APIError::CONNECTION_CLOSED) {
        ESP_LOGW(TAG, "%s: Connection closed", this->client_combined_info_.c_str());
      } else {
        ESP_LOGW(TAG, "%s: Reading failed: %s errno=%d", this->client_combined_info_.c_str(), api_error_to_str(err),
                 errno);
      }
      return;
    } else {
      this->last_traffic_ = App.get_loop_component_start_time();
      // read a packet
      if (buffer.data_len > 0) {
        this->read_message(buffer.data_len, buffer.type, &buffer.container[buffer.data_offset]);
      } else {
        this->read_message(0, buffer.type, nullptr);
      }
      if (this->remove_)
        return;
    }
  }

  // Process deferred batch if scheduled
  if (this->deferred_batch_.batch_scheduled &&
      App.get_loop_component_start_time() - this->deferred_batch_.batch_start_time >= BATCH_DELAY_MS) {
    this->process_batch_();
  }

  if (!this->list_entities_iterator_.completed())
    this->list_entities_iterator_.advance();
  if (!this->initial_state_iterator_.completed() && this->list_entities_iterator_.completed())
    this->initial_state_iterator_.advance();

  static uint8_t max_ping_retries = 60;
  static uint16_t ping_retry_interval = 1000;
  const uint32_t now = App.get_loop_component_start_time();
  if (this->sent_ping_) {
    // Disconnect if not responded within 2.5*keepalive
    if (now - this->last_traffic_ > (KEEPALIVE_TIMEOUT_MS * 5) / 2) {
      on_fatal_error();
      ESP_LOGW(TAG, "%s is unresponsive; disconnecting", this->client_combined_info_.c_str());
    }
  } else if (now - this->last_traffic_ > KEEPALIVE_TIMEOUT_MS && now > this->next_ping_retry_) {
    ESP_LOGVV(TAG, "Sending keepalive PING");
    this->sent_ping_ = this->send_message(PingRequest());
    if (!this->sent_ping_) {
      this->next_ping_retry_ = now + ping_retry_interval;
      this->ping_retries_++;
      std::string warn_str = str_sprintf("%s: Sending keepalive failed %u time(s);",
                                         this->client_combined_info_.c_str(), this->ping_retries_);
      if (this->ping_retries_ >= max_ping_retries) {
        on_fatal_error();
        ESP_LOGE(TAG, "%s disconnecting", warn_str.c_str());
      } else if (this->ping_retries_ >= 10) {
        ESP_LOGW(TAG, "%s retrying in %u ms", warn_str.c_str(), ping_retry_interval);
      } else {
        ESP_LOGD(TAG, "%s retrying in %u ms", warn_str.c_str(), ping_retry_interval);
      }
    }
  }

#ifdef USE_ESP32_CAMERA
  if (this->image_reader_.available() && this->helper_->can_write_without_blocking()) {
    // Message will use 8 more bytes than the minimum size, and typical
    // MTU is 1500. Sometimes users will see as low as 1460 MTU.
    // If its IPv6 the header is 40 bytes, and if its IPv4
    // the header is 20 bytes. So we have 1460 - 40 = 1420 bytes
    // available for the payload. But we also need to add the size of
    // the protobuf overhead, which is 8 bytes.
    //
    // To be safe we pick 1390 bytes as the maximum size
    // to send in one go. This is the maximum size of a single packet
    // that can be sent over the network.
    // This is to avoid fragmentation of the packet.
    uint32_t to_send = std::min((size_t) 1390, this->image_reader_.available());
    bool done = this->image_reader_.available() == to_send;
    uint32_t msg_size = 0;
    ProtoSize::add_fixed_field<4>(msg_size, 1, true);
    // partial message size calculated manually since its a special case
    // 1 for the data field, varint for the data size, and the data itself
    msg_size += 1 + ProtoSize::varint(to_send) + to_send;
    ProtoSize::add_bool_field(msg_size, 1, done);

    auto buffer = this->create_buffer(msg_size);
    // fixed32 key = 1;
    buffer.encode_fixed32(1, esp32_camera::global_esp32_camera->get_object_id_hash());
    // bytes data = 2;
    buffer.encode_bytes(2, this->image_reader_.peek_data_buffer(), to_send);
    // bool done = 3;
    buffer.encode_bool(3, done);

    bool success = this->send_buffer(buffer, 44);

    if (success) {
      this->image_reader_.consume_data(to_send);
    }
    if (success && done) {
      this->image_reader_.return_image();
    }
  }
#endif

  if (state_subs_at_ != -1) {
    const auto &subs = this->parent_->get_state_subs();
    if (state_subs_at_ >= (int) subs.size()) {
      state_subs_at_ = -1;
    } else {
      auto &it = subs[state_subs_at_];
      SubscribeHomeAssistantStateResponse resp;
      resp.entity_id = it.entity_id;
      resp.attribute = it.attribute.value();
      resp.once = it.once;
      if (this->send_message(resp)) {
        state_subs_at_++;
      }
    }
  }
}

std::string get_default_unique_id(const std::string &component_type, EntityBase *entity) {
  return App.get_name() + component_type + entity->get_object_id();
}

DisconnectResponse APIConnection::disconnect(const DisconnectRequest &msg) {
  // remote initiated disconnect_client
  // don't close yet, we still need to send the disconnect response
  // close will happen on next loop
  ESP_LOGD(TAG, "%s disconnected", this->client_combined_info_.c_str());
  this->next_close_ = true;
  DisconnectResponse resp;
  return resp;
}
void APIConnection::on_disconnect_response(const DisconnectResponse &value) {
  // pass
}

#ifdef USE_BINARY_SENSOR
bool APIConnection::send_binary_sensor_state(binary_sensor::BinarySensor *binary_sensor, bool state) {
  return this->schedule_message_(binary_sensor, [state](EntityBase *entity) -> std::unique_ptr<ProtoMessage> {
    auto *bs = static_cast<binary_sensor::BinarySensor *>(entity);
    auto msg = std::make_unique<BinarySensorStateResponse>();
    msg->state = state;
    msg->missing_state = !bs->has_state();
    msg->key = bs->get_object_id_hash();
    return msg;
  });
}
void APIConnection::send_binary_sensor_info(binary_sensor::BinarySensor *binary_sensor) {
  this->schedule_message_(binary_sensor, &APIConnection::try_send_binary_sensor_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_binary_sensor_info_(EntityBase *entity) {
  auto *binary_sensor = static_cast<binary_sensor::BinarySensor *>(entity);
  auto msg = std::make_unique<ListEntitiesBinarySensorResponse>();
  msg->device_class = binary_sensor->get_device_class();
  msg->is_status_binary_sensor = binary_sensor->is_status_binary_sensor();
  msg->unique_id = get_default_unique_id("binary_sensor", binary_sensor);

  // Fill common entity fields
  fill_entity_info_base_(binary_sensor, *msg);

  return msg;
}
#endif

#ifdef USE_COVER
bool APIConnection::send_cover_state(cover::Cover *cover) {
  return this->schedule_message_(cover, &APIConnection::try_send_cover_state_);
}
void APIConnection::send_cover_info(cover::Cover *cover) {
  this->schedule_message_(cover, &APIConnection::try_send_cover_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_cover_state_(EntityBase *entity) {
  auto *cover = static_cast<cover::Cover *>(entity);
  auto msg = std::make_unique<CoverStateResponse>();
  auto traits = cover->get_traits();
  msg->legacy_state =
      (cover->position == cover::COVER_OPEN) ? enums::LEGACY_COVER_STATE_OPEN : enums::LEGACY_COVER_STATE_CLOSED;
  msg->position = cover->position;
  if (traits.get_supports_tilt())
    msg->tilt = cover->tilt;
  msg->current_operation = static_cast<enums::CoverOperation>(cover->current_operation);
  msg->key = cover->get_object_id_hash();
  return msg;
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_cover_info_(EntityBase *entity) {
  auto *cover = static_cast<cover::Cover *>(entity);
  auto msg = std::make_unique<ListEntitiesCoverResponse>();
  auto traits = cover->get_traits();
  msg->assumed_state = traits.get_is_assumed_state();
  msg->supports_position = traits.get_supports_position();
  msg->supports_tilt = traits.get_supports_tilt();
  msg->supports_stop = traits.get_supports_stop();
  msg->device_class = cover->get_device_class();
  msg->unique_id = get_default_unique_id("cover", cover);
  fill_entity_info_base_(cover, *msg);
  return msg;
}
void APIConnection::cover_command(const CoverCommandRequest &msg) {
  cover::Cover *cover = App.get_cover_by_key(msg.key);
  if (cover == nullptr)
    return;

  auto call = cover->make_call();
  if (msg.has_legacy_command) {
    switch (msg.legacy_command) {
      case enums::LEGACY_COVER_COMMAND_OPEN:
        call.set_command_open();
        break;
      case enums::LEGACY_COVER_COMMAND_CLOSE:
        call.set_command_close();
        break;
      case enums::LEGACY_COVER_COMMAND_STOP:
        call.set_command_stop();
        break;
    }
  }
  if (msg.has_position)
    call.set_position(msg.position);
  if (msg.has_tilt)
    call.set_tilt(msg.tilt);
  if (msg.stop)
    call.set_command_stop();
  call.perform();
}
#endif

#ifdef USE_FAN
bool APIConnection::send_fan_state(fan::Fan *fan) {
  return this->schedule_message_(fan, &APIConnection::try_send_fan_state_);
}
void APIConnection::send_fan_info(fan::Fan *fan) { this->schedule_message_(fan, &APIConnection::try_send_fan_info_); }
std::unique_ptr<ProtoMessage> APIConnection::try_send_fan_state_(EntityBase *entity) {
  auto *fan = static_cast<fan::Fan *>(entity);
  auto msg = std::make_unique<FanStateResponse>();
  auto traits = fan->get_traits();
  msg->state = fan->state;
  if (traits.supports_oscillation())
    msg->oscillating = fan->oscillating;
  if (traits.supports_speed()) {
    msg->speed_level = fan->speed;
  }
  if (traits.supports_direction())
    msg->direction = static_cast<enums::FanDirection>(fan->direction);
  if (traits.supports_preset_modes())
    msg->preset_mode = fan->preset_mode;
  msg->key = fan->get_object_id_hash();
  return msg;
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_fan_info_(EntityBase *entity) {
  auto *fan = static_cast<fan::Fan *>(entity);
  auto msg = std::make_unique<ListEntitiesFanResponse>();
  auto traits = fan->get_traits();
  msg->supports_oscillation = traits.supports_oscillation();
  msg->supports_speed = traits.supports_speed();
  msg->supports_direction = traits.supports_direction();
  msg->supported_speed_count = traits.supported_speed_count();
  for (auto const &preset : traits.supported_preset_modes())
    msg->supported_preset_modes.push_back(preset);
  msg->unique_id = get_default_unique_id("fan", fan);
  fill_entity_info_base_(fan, *msg);
  return msg;
}
void APIConnection::fan_command(const FanCommandRequest &msg) {
  fan::Fan *fan = App.get_fan_by_key(msg.key);
  if (fan == nullptr)
    return;

  auto call = fan->make_call();
  if (msg.has_state)
    call.set_state(msg.state);
  if (msg.has_oscillating)
    call.set_oscillating(msg.oscillating);
  if (msg.has_speed_level) {
    // Prefer level
    call.set_speed(msg.speed_level);
  }
  if (msg.has_direction)
    call.set_direction(static_cast<fan::FanDirection>(msg.direction));
  if (msg.has_preset_mode)
    call.set_preset_mode(msg.preset_mode);
  call.perform();
}
#endif

#ifdef USE_LIGHT
bool APIConnection::send_light_state(light::LightState *light) {
  return this->schedule_message_(light, &APIConnection::try_send_light_state_);
}
void APIConnection::send_light_info(light::LightState *light) {
  this->schedule_message_(light, &APIConnection::try_send_light_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_light_state_(EntityBase *entity) {
  auto *light = static_cast<light::LightState *>(entity);
  auto msg = std::make_unique<LightStateResponse>();
  auto traits = light->get_traits();
  auto values = light->remote_values;
  auto color_mode = values.get_color_mode();
  msg->state = values.is_on();
  msg->color_mode = static_cast<enums::ColorMode>(color_mode);
  msg->brightness = values.get_brightness();
  msg->color_brightness = values.get_color_brightness();
  msg->red = values.get_red();
  msg->green = values.get_green();
  msg->blue = values.get_blue();
  msg->white = values.get_white();
  msg->color_temperature = values.get_color_temperature();
  msg->cold_white = values.get_cold_white();
  msg->warm_white = values.get_warm_white();
  if (light->supports_effects())
    msg->effect = light->get_effect_name();
  msg->key = light->get_object_id_hash();
  return msg;
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_light_info_(EntityBase *entity) {
  auto *light = static_cast<light::LightState *>(entity);
  auto msg = std::make_unique<ListEntitiesLightResponse>();
  auto traits = light->get_traits();
  for (auto mode : traits.get_supported_color_modes())
    msg->supported_color_modes.push_back(static_cast<enums::ColorMode>(mode));
  msg->legacy_supports_brightness = traits.supports_color_capability(light::ColorCapability::BRIGHTNESS);
  msg->legacy_supports_rgb = traits.supports_color_capability(light::ColorCapability::RGB);
  msg->legacy_supports_white_value =
      msg->legacy_supports_rgb && (traits.supports_color_capability(light::ColorCapability::WHITE) ||
                                   traits.supports_color_capability(light::ColorCapability::COLD_WARM_WHITE));
  msg->legacy_supports_color_temperature =
      traits.supports_color_capability(light::ColorCapability::COLOR_TEMPERATURE) ||
      traits.supports_color_capability(light::ColorCapability::COLD_WARM_WHITE);
  if (msg->legacy_supports_color_temperature) {
    msg->min_mireds = traits.get_min_mireds();
    msg->max_mireds = traits.get_max_mireds();
  }
  if (light->supports_effects()) {
    msg->effects.emplace_back("None");
    for (auto *effect : light->get_effects()) {
      msg->effects.push_back(effect->get_name());
    }
  }
  msg->unique_id = get_default_unique_id("light", light);
  fill_entity_info_base_(light, *msg);
  return msg;
}
void APIConnection::light_command(const LightCommandRequest &msg) {
  light::LightState *light = App.get_light_by_key(msg.key);
  if (light == nullptr)
    return;

  auto call = light->make_call();
  if (msg.has_state)
    call.set_state(msg.state);
  if (msg.has_brightness)
    call.set_brightness(msg.brightness);
  if (msg.has_color_mode)
    call.set_color_mode(static_cast<light::ColorMode>(msg.color_mode));
  if (msg.has_color_brightness)
    call.set_color_brightness(msg.color_brightness);
  if (msg.has_rgb) {
    call.set_red(msg.red);
    call.set_green(msg.green);
    call.set_blue(msg.blue);
  }
  if (msg.has_white)
    call.set_white(msg.white);
  if (msg.has_color_temperature)
    call.set_color_temperature(msg.color_temperature);
  if (msg.has_cold_white)
    call.set_cold_white(msg.cold_white);
  if (msg.has_warm_white)
    call.set_warm_white(msg.warm_white);
  if (msg.has_transition_length)
    call.set_transition_length(msg.transition_length);
  if (msg.has_flash_length)
    call.set_flash_length(msg.flash_length);
  if (msg.has_effect)
    call.set_effect(msg.effect);
  call.perform();
}
#endif

#ifdef USE_SENSOR
bool APIConnection::send_sensor_state(sensor::Sensor *sensor, float state) {
  return this->schedule_message_(sensor, [state](EntityBase *entity) -> std::unique_ptr<ProtoMessage> {
    auto *s = static_cast<sensor::Sensor *>(entity);
    auto msg = std::make_unique<SensorStateResponse>();
    msg->state = state;
    msg->missing_state = !s->has_state();
    msg->key = s->get_object_id_hash();
    return msg;
  });
}
void APIConnection::send_sensor_info(sensor::Sensor *sensor) {
  this->schedule_message_(sensor, &APIConnection::try_send_sensor_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_sensor_info_(EntityBase *entity) {
  auto *sensor = static_cast<sensor::Sensor *>(entity);
  auto msg = std::make_unique<ListEntitiesSensorResponse>();
  msg->unit_of_measurement = sensor->get_unit_of_measurement();
  msg->accuracy_decimals = sensor->get_accuracy_decimals();
  msg->force_update = sensor->get_force_update();
  msg->device_class = sensor->get_device_class();
  msg->state_class = static_cast<enums::SensorStateClass>(sensor->get_state_class());
  msg->unique_id = sensor->unique_id();
  if (msg->unique_id.empty())
    msg->unique_id = get_default_unique_id("sensor", sensor);
  fill_entity_info_base_(sensor, *msg);
  return msg;
}
#endif

#ifdef USE_SWITCH
bool APIConnection::send_switch_state(switch_::Switch *a_switch, bool state) {
  return this->schedule_message_(a_switch, [state](EntityBase *entity) -> std::unique_ptr<ProtoMessage> {
    auto *sw = static_cast<switch_::Switch *>(entity);
    auto msg = std::make_unique<SwitchStateResponse>();
    msg->state = state;
    msg->key = sw->get_object_id_hash();
    return msg;
  });
}
void APIConnection::send_switch_info(switch_::Switch *a_switch) {
  this->schedule_message_(a_switch, &APIConnection::try_send_switch_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_switch_info_(EntityBase *entity) {
  auto *a_switch = static_cast<switch_::Switch *>(entity);
  auto msg = std::make_unique<ListEntitiesSwitchResponse>();
  msg->assumed_state = a_switch->assumed_state();
  msg->device_class = a_switch->get_device_class();
  msg->unique_id = get_default_unique_id("switch", a_switch);
  fill_entity_info_base_(a_switch, *msg);
  return msg;
}
void APIConnection::switch_command(const SwitchCommandRequest &msg) {
  switch_::Switch *a_switch = App.get_switch_by_key(msg.key);
  if (a_switch == nullptr)
    return;

  if (msg.state) {
    a_switch->turn_on();
  } else {
    a_switch->turn_off();
  }
}
#endif

#ifdef USE_TEXT_SENSOR
bool APIConnection::send_text_sensor_state(text_sensor::TextSensor *text_sensor, std::string state) {
  return this->schedule_message_(text_sensor,
                                 [state = std::move(state)](EntityBase *entity) -> std::unique_ptr<ProtoMessage> {
                                   auto *ts = static_cast<text_sensor::TextSensor *>(entity);
                                   auto msg = std::make_unique<TextSensorStateResponse>();
                                   msg->state = std::move(state);
                                   msg->missing_state = !ts->has_state();
                                   msg->key = ts->get_object_id_hash();
                                   return msg;
                                 });
}
void APIConnection::send_text_sensor_info(text_sensor::TextSensor *text_sensor) {
  this->schedule_message_(text_sensor, &APIConnection::try_send_text_sensor_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_text_sensor_info_(EntityBase *entity) {
  auto *text_sensor = static_cast<text_sensor::TextSensor *>(entity);
  auto msg = std::make_unique<ListEntitiesTextSensorResponse>();
  msg->device_class = text_sensor->get_device_class();
  msg->unique_id = text_sensor->unique_id();
  if (msg->unique_id.empty())
    msg->unique_id = get_default_unique_id("text_sensor", text_sensor);

  // Fill common entity fields
  fill_entity_info_base_(text_sensor, *msg);

  return msg;
}
#endif

#ifdef USE_CLIMATE
bool APIConnection::send_climate_state(climate::Climate *climate) {
  return this->schedule_message_(climate, &APIConnection::try_send_climate_state_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_climate_state_(EntityBase *entity) {
  auto *climate = static_cast<climate::Climate *>(entity);
  auto msg = std::make_unique<ClimateStateResponse>();
  msg->key = climate->get_object_id_hash();
  auto traits = climate->get_traits();
  msg->mode = static_cast<enums::ClimateMode>(climate->mode);
  msg->action = static_cast<enums::ClimateAction>(climate->action);
  if (traits.get_supports_current_temperature())
    msg->current_temperature = climate->current_temperature;
  if (traits.get_supports_two_point_target_temperature()) {
    msg->target_temperature_low = climate->target_temperature_low;
    msg->target_temperature_high = climate->target_temperature_high;
  } else {
    msg->target_temperature = climate->target_temperature;
  }
  if (traits.get_supports_fan_modes() && climate->fan_mode.has_value())
    msg->fan_mode = static_cast<enums::ClimateFanMode>(climate->fan_mode.value());
  if (!traits.get_supported_custom_fan_modes().empty() && climate->custom_fan_mode.has_value())
    msg->custom_fan_mode = climate->custom_fan_mode.value();
  if (traits.get_supports_presets() && climate->preset.has_value()) {
    msg->preset = static_cast<enums::ClimatePreset>(climate->preset.value());
  }
  if (!traits.get_supported_custom_presets().empty() && climate->custom_preset.has_value())
    msg->custom_preset = climate->custom_preset.value();
  if (traits.get_supports_swing_modes())
    msg->swing_mode = static_cast<enums::ClimateSwingMode>(climate->swing_mode);
  if (traits.get_supports_current_humidity())
    msg->current_humidity = climate->current_humidity;
  if (traits.get_supports_target_humidity())
    msg->target_humidity = climate->target_humidity;
  return msg;
}
void APIConnection::send_climate_info(climate::Climate *climate) {
  this->schedule_message_(climate, &APIConnection::try_send_climate_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_climate_info_(EntityBase *entity) {
  auto *climate = static_cast<climate::Climate *>(entity);
  auto msg = std::make_unique<ListEntitiesClimateResponse>();
  auto traits = climate->get_traits();
  msg->supports_current_temperature = traits.get_supports_current_temperature();
  msg->supports_current_humidity = traits.get_supports_current_humidity();
  msg->supports_two_point_target_temperature = traits.get_supports_two_point_target_temperature();
  msg->supports_target_humidity = traits.get_supports_target_humidity();
  for (auto mode : traits.get_supported_modes())
    msg->supported_modes.push_back(static_cast<enums::ClimateMode>(mode));
  msg->visual_min_temperature = traits.get_visual_min_temperature();
  msg->visual_max_temperature = traits.get_visual_max_temperature();
  msg->visual_target_temperature_step = traits.get_visual_target_temperature_step();
  msg->visual_current_temperature_step = traits.get_visual_current_temperature_step();
  msg->visual_min_humidity = traits.get_visual_min_humidity();
  msg->visual_max_humidity = traits.get_visual_max_humidity();
  msg->legacy_supports_away = traits.supports_preset(climate::CLIMATE_PRESET_AWAY);
  msg->supports_action = traits.get_supports_action();
  for (auto fan_mode : traits.get_supported_fan_modes())
    msg->supported_fan_modes.push_back(static_cast<enums::ClimateFanMode>(fan_mode));
  for (auto const &custom_fan_mode : traits.get_supported_custom_fan_modes())
    msg->supported_custom_fan_modes.push_back(custom_fan_mode);
  for (auto preset : traits.get_supported_presets())
    msg->supported_presets.push_back(static_cast<enums::ClimatePreset>(preset));
  for (auto const &custom_preset : traits.get_supported_custom_presets())
    msg->supported_custom_presets.push_back(custom_preset);
  for (auto swing_mode : traits.get_supported_swing_modes())
    msg->supported_swing_modes.push_back(static_cast<enums::ClimateSwingMode>(swing_mode));
  msg->unique_id = get_default_unique_id("climate", climate);

  // Fill common entity fields
  fill_entity_info_base_(climate, *msg);

  return msg;
}
void APIConnection::climate_command(const ClimateCommandRequest &msg) {
  climate::Climate *climate = App.get_climate_by_key(msg.key);
  if (climate == nullptr)
    return;

  auto call = climate->make_call();
  if (msg.has_mode)
    call.set_mode(static_cast<climate::ClimateMode>(msg.mode));
  if (msg.has_target_temperature)
    call.set_target_temperature(msg.target_temperature);
  if (msg.has_target_temperature_low)
    call.set_target_temperature_low(msg.target_temperature_low);
  if (msg.has_target_temperature_high)
    call.set_target_temperature_high(msg.target_temperature_high);
  if (msg.has_target_humidity)
    call.set_target_humidity(msg.target_humidity);
  if (msg.has_fan_mode)
    call.set_fan_mode(static_cast<climate::ClimateFanMode>(msg.fan_mode));
  if (msg.has_custom_fan_mode)
    call.set_fan_mode(msg.custom_fan_mode);
  if (msg.has_preset)
    call.set_preset(static_cast<climate::ClimatePreset>(msg.preset));
  if (msg.has_custom_preset)
    call.set_preset(msg.custom_preset);
  if (msg.has_swing_mode)
    call.set_swing_mode(static_cast<climate::ClimateSwingMode>(msg.swing_mode));
  call.perform();
}
#endif

#ifdef USE_NUMBER
bool APIConnection::send_number_state(number::Number *number, float state) {
  return this->schedule_message_(number, [state](EntityBase *entity) -> std::unique_ptr<ProtoMessage> {
    auto *n = static_cast<number::Number *>(entity);
    auto msg = std::make_unique<NumberStateResponse>();
    msg->state = state;
    msg->missing_state = !n->has_state();
    msg->key = n->get_object_id_hash();
    return msg;
  });
}
void APIConnection::send_number_info(number::Number *number) {
  this->schedule_message_(number, &APIConnection::try_send_number_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_number_info_(EntityBase *entity) {
  auto *number = static_cast<number::Number *>(entity);
  auto msg = std::make_unique<ListEntitiesNumberResponse>();
  msg->unit_of_measurement = number->traits.get_unit_of_measurement();
  msg->mode = static_cast<enums::NumberMode>(number->traits.get_mode());
  msg->device_class = number->traits.get_device_class();
  msg->min_value = number->traits.get_min_value();
  msg->max_value = number->traits.get_max_value();
  msg->step = number->traits.get_step();
  msg->unique_id = get_default_unique_id("number", number);

  // Fill common entity fields
  fill_entity_info_base_(number, *msg);

  return msg;
}
void APIConnection::number_command(const NumberCommandRequest &msg) {
  number::Number *number = App.get_number_by_key(msg.key);
  if (number == nullptr)
    return;

  auto call = number->make_call();
  call.set_value(msg.state);
  call.perform();
}
#endif

#ifdef USE_DATETIME_DATE
bool APIConnection::send_date_state(datetime::DateEntity *date) {
  return this->schedule_message_(date, &APIConnection::try_send_date_state_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_date_state_(EntityBase *entity) {
  auto *date = static_cast<datetime::DateEntity *>(entity);
  auto msg = std::make_unique<DateStateResponse>();
  msg->missing_state = !date->has_state();
  msg->year = date->year;
  msg->month = date->month;
  msg->day = date->day;
  msg->key = date->get_object_id_hash();
  return msg;
}
void APIConnection::send_date_info(datetime::DateEntity *date) {
  this->schedule_message_(date, &APIConnection::try_send_date_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_date_info_(EntityBase *entity) {
  auto *date = static_cast<datetime::DateEntity *>(entity);
  auto msg = std::make_unique<ListEntitiesDateResponse>();
  msg->unique_id = get_default_unique_id("date", date);

  // Fill common entity fields
  fill_entity_info_base_(date, *msg);

  return msg;
}
void APIConnection::date_command(const DateCommandRequest &msg) {
  datetime::DateEntity *date = App.get_date_by_key(msg.key);
  if (date == nullptr)
    return;

  auto call = date->make_call();
  call.set_date(msg.year, msg.month, msg.day);
  call.perform();
}
#endif

#ifdef USE_DATETIME_TIME
bool APIConnection::send_time_state(datetime::TimeEntity *time) {
  return this->schedule_message_(time, &APIConnection::try_send_time_state_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_time_state_(EntityBase *entity) {
  auto *time = static_cast<datetime::TimeEntity *>(entity);
  auto msg = std::make_unique<TimeStateResponse>();
  msg->missing_state = !time->has_state();
  msg->hour = time->hour;
  msg->minute = time->minute;
  msg->second = time->second;
  msg->key = time->get_object_id_hash();
  return msg;
}
void APIConnection::send_time_info(datetime::TimeEntity *time) {
  this->schedule_message_(time, &APIConnection::try_send_time_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_time_info_(EntityBase *entity) {
  auto *time = static_cast<datetime::TimeEntity *>(entity);
  auto msg = std::make_unique<ListEntitiesTimeResponse>();
  msg->unique_id = get_default_unique_id("time", time);

  // Fill common entity fields
  fill_entity_info_base_(time, *msg);

  return msg;
}
void APIConnection::time_command(const TimeCommandRequest &msg) {
  datetime::TimeEntity *time = App.get_time_by_key(msg.key);
  if (time == nullptr)
    return;

  auto call = time->make_call();
  call.set_time(msg.hour, msg.minute, msg.second);
  call.perform();
}
#endif

#ifdef USE_DATETIME_DATETIME
bool APIConnection::send_datetime_state(datetime::DateTimeEntity *datetime) {
  return this->schedule_message_(datetime, &APIConnection::try_send_datetime_state_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_datetime_state_(EntityBase *entity) {
  auto *datetime = static_cast<datetime::DateTimeEntity *>(entity);
  auto msg = std::make_unique<DateTimeStateResponse>();
  msg->missing_state = !datetime->has_state();
  if (datetime->has_state()) {
    ESPTime state = datetime->state_as_esptime();
    msg->epoch_seconds = state.timestamp;
  }
  msg->key = datetime->get_object_id_hash();
  return msg;
}
void APIConnection::send_datetime_info(datetime::DateTimeEntity *datetime) {
  this->schedule_message_(datetime, &APIConnection::try_send_datetime_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_datetime_info_(EntityBase *entity) {
  auto *datetime = static_cast<datetime::DateTimeEntity *>(entity);
  auto msg = std::make_unique<ListEntitiesDateTimeResponse>();
  msg->unique_id = get_default_unique_id("datetime", datetime);

  // Fill common entity fields
  fill_entity_info_base_(datetime, *msg);

  return msg;
}
void APIConnection::datetime_command(const DateTimeCommandRequest &msg) {
  datetime::DateTimeEntity *datetime = App.get_datetime_by_key(msg.key);
  if (datetime == nullptr)
    return;

  auto call = datetime->make_call();
  call.set_datetime(msg.epoch_seconds);
  call.perform();
}
#endif

#ifdef USE_TEXT
bool APIConnection::send_text_state(text::Text *text, std::string state) {
  return this->schedule_message_(text, [state = std::move(state)](EntityBase *entity) -> std::unique_ptr<ProtoMessage> {
    auto *t = static_cast<text::Text *>(entity);
    auto msg = std::make_unique<TextStateResponse>();
    msg->state = std::move(state);
    msg->missing_state = !t->has_state();
    msg->key = t->get_object_id_hash();
    return msg;
  });
}
void APIConnection::send_text_info(text::Text *text) {
  this->schedule_message_(text, &APIConnection::try_send_text_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_text_info_(EntityBase *entity) {
  auto *text = static_cast<text::Text *>(entity);
  auto msg = std::make_unique<ListEntitiesTextResponse>();
  msg->mode = static_cast<enums::TextMode>(text->traits.get_mode());
  msg->min_length = text->traits.get_min_length();
  msg->max_length = text->traits.get_max_length();
  msg->pattern = text->traits.get_pattern();
  msg->unique_id = get_default_unique_id("text", text);

  // Fill common entity fields
  fill_entity_info_base_(text, *msg);

  return msg;
}
void APIConnection::text_command(const TextCommandRequest &msg) {
  text::Text *text = App.get_text_by_key(msg.key);
  if (text == nullptr)
    return;

  auto call = text->make_call();
  call.set_value(msg.state);
  call.perform();
}
#endif

#ifdef USE_SELECT
bool APIConnection::send_select_state(select::Select *select, std::string state) {
  return this->schedule_message_(select,
                                 [state = std::move(state)](EntityBase *entity) -> std::unique_ptr<ProtoMessage> {
                                   auto *s = static_cast<select::Select *>(entity);
                                   auto msg = std::make_unique<SelectStateResponse>();
                                   msg->state = std::move(state);
                                   msg->missing_state = !s->has_state();
                                   msg->key = s->get_object_id_hash();
                                   return msg;
                                 });
}
void APIConnection::send_select_info(select::Select *select) {
  this->schedule_message_(select, &APIConnection::try_send_select_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_select_info_(EntityBase *entity) {
  auto *select = static_cast<select::Select *>(entity);
  auto msg = std::make_unique<ListEntitiesSelectResponse>();
  for (const auto &option : select->traits.get_options())
    msg->options.push_back(option);
  msg->unique_id = get_default_unique_id("select", select);

  // Fill common entity fields
  fill_entity_info_base_(select, *msg);

  return msg;
}
void APIConnection::select_command(const SelectCommandRequest &msg) {
  select::Select *select = App.get_select_by_key(msg.key);
  if (select == nullptr)
    return;

  auto call = select->make_call();
  call.set_option(msg.state);
  call.perform();
}
#endif

#ifdef USE_BUTTON
void esphome::api::APIConnection::send_button_info(button::Button *button) {
  this->schedule_message_(button, &APIConnection::try_send_button_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_button_info_(EntityBase *entity) {
  auto *button = static_cast<button::Button *>(entity);
  auto msg = std::make_unique<ListEntitiesButtonResponse>();
  msg->device_class = button->get_device_class();
  msg->unique_id = get_default_unique_id("button", button);

  // Fill common entity fields
  fill_entity_info_base_(button, *msg);

  return msg;
}
void esphome::api::APIConnection::button_command(const ButtonCommandRequest &msg) {
  button::Button *button = App.get_button_by_key(msg.key);
  if (button == nullptr)
    return;

  button->press();
}
#endif

#ifdef USE_LOCK
bool APIConnection::send_lock_state(lock::Lock *a_lock, lock::LockState state) {
  return this->schedule_message_(a_lock, [state](EntityBase *entity) -> std::unique_ptr<ProtoMessage> {
    auto *l = static_cast<lock::Lock *>(entity);
    auto msg = std::make_unique<LockStateResponse>();
    msg->state = static_cast<enums::LockState>(state);
    msg->key = l->get_object_id_hash();
    return msg;
  });
}
void APIConnection::send_lock_info(lock::Lock *a_lock) {
  this->schedule_message_(a_lock, &APIConnection::try_send_lock_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_lock_info_(EntityBase *entity) {
  auto *a_lock = static_cast<lock::Lock *>(entity);
  auto msg = std::make_unique<ListEntitiesLockResponse>();
  msg->assumed_state = a_lock->traits.get_assumed_state();
  msg->supports_open = a_lock->traits.get_supports_open();
  msg->requires_code = a_lock->traits.get_requires_code();
  msg->unique_id = get_default_unique_id("lock", a_lock);

  // Fill common entity fields
  fill_entity_info_base_(a_lock, *msg);

  return msg;
}
void APIConnection::lock_command(const LockCommandRequest &msg) {
  lock::Lock *a_lock = App.get_lock_by_key(msg.key);
  if (a_lock == nullptr)
    return;

  switch (msg.command) {
    case enums::LOCK_UNLOCK:
      a_lock->unlock();
      break;
    case enums::LOCK_LOCK:
      a_lock->lock();
      break;
    case enums::LOCK_OPEN:
      a_lock->open();
      break;
  }
}
#endif

#ifdef USE_VALVE
bool APIConnection::send_valve_state(valve::Valve *valve) {
  return this->schedule_message_(valve, &APIConnection::try_send_valve_state_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_valve_state_(EntityBase *entity) {
  auto *valve = static_cast<valve::Valve *>(entity);
  auto msg = std::make_unique<ValveStateResponse>();
  msg->position = valve->position;
  msg->current_operation = static_cast<enums::ValveOperation>(valve->current_operation);
  msg->key = valve->get_object_id_hash();
  return msg;
}
void APIConnection::send_valve_info(valve::Valve *valve) {
  this->schedule_message_(valve, &APIConnection::try_send_valve_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_valve_info_(EntityBase *entity) {
  auto *valve = static_cast<valve::Valve *>(entity);
  auto msg = std::make_unique<ListEntitiesValveResponse>();
  auto traits = valve->get_traits();
  msg->device_class = valve->get_device_class();
  msg->assumed_state = traits.get_is_assumed_state();
  msg->supports_position = traits.get_supports_position();
  msg->supports_stop = traits.get_supports_stop();
  msg->unique_id = get_default_unique_id("valve", valve);

  // Fill common entity fields
  fill_entity_info_base_(valve, *msg);

  return msg;
}
void APIConnection::valve_command(const ValveCommandRequest &msg) {
  valve::Valve *valve = App.get_valve_by_key(msg.key);
  if (valve == nullptr)
    return;

  auto call = valve->make_call();
  if (msg.has_position)
    call.set_position(msg.position);
  if (msg.stop)
    call.set_command_stop();
  call.perform();
}
#endif

#ifdef USE_MEDIA_PLAYER
bool APIConnection::send_media_player_state(media_player::MediaPlayer *media_player) {
  return this->schedule_message_(media_player, &APIConnection::try_send_media_player_state_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_media_player_state_(EntityBase *entity) {
  auto *media_player = static_cast<media_player::MediaPlayer *>(entity);
  auto msg = std::make_unique<MediaPlayerStateResponse>();
  media_player::MediaPlayerState report_state = media_player->state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING
                                                    ? media_player::MEDIA_PLAYER_STATE_PLAYING
                                                    : media_player->state;
  msg->state = static_cast<enums::MediaPlayerState>(report_state);
  msg->volume = media_player->volume;
  msg->muted = media_player->is_muted();
  msg->key = media_player->get_object_id_hash();
  return msg;
}
void APIConnection::send_media_player_info(media_player::MediaPlayer *media_player) {
  this->schedule_message_(media_player, &APIConnection::try_send_media_player_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_media_player_info_(EntityBase *entity) {
  auto *media_player = static_cast<media_player::MediaPlayer *>(entity);
  auto msg = std::make_unique<ListEntitiesMediaPlayerResponse>();
  auto traits = media_player->get_traits();
  msg->supports_pause = traits.get_supports_pause();
  for (auto &supported_format : traits.get_supported_formats()) {
    MediaPlayerSupportedFormat media_format;
    media_format.format = supported_format.format;
    media_format.sample_rate = supported_format.sample_rate;
    media_format.num_channels = supported_format.num_channels;
    media_format.purpose = static_cast<enums::MediaPlayerFormatPurpose>(supported_format.purpose);
    media_format.sample_bytes = supported_format.sample_bytes;
    msg->supported_formats.push_back(media_format);
  }
  msg->unique_id = get_default_unique_id("media_player", media_player);

  // Fill common entity fields
  fill_entity_info_base_(media_player, *msg);

  return msg;
}
void APIConnection::media_player_command(const MediaPlayerCommandRequest &msg) {
  media_player::MediaPlayer *media_player = App.get_media_player_by_key(msg.key);
  if (media_player == nullptr)
    return;

  auto call = media_player->make_call();
  if (msg.has_command) {
    call.set_command(static_cast<media_player::MediaPlayerCommand>(msg.command));
  }
  if (msg.has_volume) {
    call.set_volume(msg.volume);
  }
  if (msg.has_media_url) {
    call.set_media_url(msg.media_url);
  }
  if (msg.has_announcement) {
    call.set_announcement(msg.announcement);
  }
  call.perform();
}
#endif

#ifdef USE_ESP32_CAMERA
void APIConnection::set_camera_state(std::shared_ptr<esp32_camera::CameraImage> image) {
  if (!this->state_subscription_)
    return;
  if (this->image_reader_.available())
    return;
  if (image->was_requested_by(esphome::esp32_camera::API_REQUESTER) ||
      image->was_requested_by(esphome::esp32_camera::IDLE))
    this->image_reader_.set_image(std::move(image));
}
void APIConnection::send_camera_info(esp32_camera::ESP32Camera *camera) {
  this->schedule_message_(camera, &APIConnection::try_send_camera_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_camera_info_(EntityBase *entity) {
  auto *camera = static_cast<esp32_camera::ESP32Camera *>(entity);
  auto msg = std::make_unique<ListEntitiesCameraResponse>();
  msg->unique_id = get_default_unique_id("camera", camera);

  // Fill common entity fields
  fill_entity_info_base_(camera, *msg);

  return msg;
}
void APIConnection::camera_image(const CameraImageRequest &msg) {
  if (esp32_camera::global_esp32_camera == nullptr)
    return;

  if (msg.single)
    esp32_camera::global_esp32_camera->request_image(esphome::esp32_camera::API_REQUESTER);
  if (msg.stream) {
    esp32_camera::global_esp32_camera->start_stream(esphome::esp32_camera::API_REQUESTER);

    App.scheduler.set_timeout(this->parent_, "api_esp32_camera_stop_stream", ESP32_CAMERA_STOP_STREAM, []() {
      esp32_camera::global_esp32_camera->stop_stream(esphome::esp32_camera::API_REQUESTER);
    });
  }
}
#endif

#ifdef USE_HOMEASSISTANT_TIME
void APIConnection::on_get_time_response(const GetTimeResponse &value) {
  if (homeassistant::global_homeassistant_time != nullptr)
    homeassistant::global_homeassistant_time->set_epoch_time(value.epoch_seconds);
}
#endif

#ifdef USE_BLUETOOTH_PROXY
void APIConnection::subscribe_bluetooth_le_advertisements(const SubscribeBluetoothLEAdvertisementsRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->subscribe_api_connection(this, msg.flags);
}
void APIConnection::unsubscribe_bluetooth_le_advertisements(const UnsubscribeBluetoothLEAdvertisementsRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->unsubscribe_api_connection(this);
}
bool APIConnection::send_bluetooth_le_advertisement(const BluetoothLEAdvertisementResponse &msg) {
  if (this->client_api_version_major_ < 1 || this->client_api_version_minor_ < 7) {
    BluetoothLEAdvertisementResponse resp = msg;
    for (auto &service : resp.service_data) {
      service.legacy_data.assign(service.data.begin(), service.data.end());
      service.data.clear();
    }
    for (auto &manufacturer_data : resp.manufacturer_data) {
      manufacturer_data.legacy_data.assign(manufacturer_data.data.begin(), manufacturer_data.data.end());
      manufacturer_data.data.clear();
    }
    return this->send_message(resp);
  }
  return this->send_message(msg);
}
void APIConnection::bluetooth_device_request(const BluetoothDeviceRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_device_request(msg);
}
void APIConnection::bluetooth_gatt_read(const BluetoothGATTReadRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_read(msg);
}
void APIConnection::bluetooth_gatt_write(const BluetoothGATTWriteRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_write(msg);
}
void APIConnection::bluetooth_gatt_read_descriptor(const BluetoothGATTReadDescriptorRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_read_descriptor(msg);
}
void APIConnection::bluetooth_gatt_write_descriptor(const BluetoothGATTWriteDescriptorRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_write_descriptor(msg);
}
void APIConnection::bluetooth_gatt_get_services(const BluetoothGATTGetServicesRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_send_services(msg);
}

void APIConnection::bluetooth_gatt_notify(const BluetoothGATTNotifyRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_notify(msg);
}

BluetoothConnectionsFreeResponse APIConnection::subscribe_bluetooth_connections_free(
    const SubscribeBluetoothConnectionsFreeRequest &msg) {
  BluetoothConnectionsFreeResponse resp;
  resp.free = bluetooth_proxy::global_bluetooth_proxy->get_bluetooth_connections_free();
  resp.limit = bluetooth_proxy::global_bluetooth_proxy->get_bluetooth_connections_limit();
  return resp;
}

void APIConnection::bluetooth_scanner_set_mode(const BluetoothScannerSetModeRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_scanner_set_mode(
      msg.mode == enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_ACTIVE);
}
#endif

#ifdef USE_VOICE_ASSISTANT
void APIConnection::subscribe_voice_assistant(const SubscribeVoiceAssistantRequest &msg) {
  if (voice_assistant::global_voice_assistant != nullptr) {
    voice_assistant::global_voice_assistant->client_subscription(this, msg.subscribe);
  }
}
void APIConnection::on_voice_assistant_response(const VoiceAssistantResponse &msg) {
  if (voice_assistant::global_voice_assistant != nullptr) {
    if (voice_assistant::global_voice_assistant->get_api_connection() != this) {
      return;
    }

    if (msg.error) {
      voice_assistant::global_voice_assistant->failed_to_start();
      return;
    }
    if (msg.port == 0) {
      // Use API Audio
      voice_assistant::global_voice_assistant->start_streaming();
    } else {
      struct sockaddr_storage storage;
      socklen_t len = sizeof(storage);
      this->helper_->getpeername((struct sockaddr *) &storage, &len);
      voice_assistant::global_voice_assistant->start_streaming(&storage, msg.port);
    }
  }
};
void APIConnection::on_voice_assistant_event_response(const VoiceAssistantEventResponse &msg) {
  if (voice_assistant::global_voice_assistant != nullptr) {
    if (voice_assistant::global_voice_assistant->get_api_connection() != this) {
      return;
    }

    voice_assistant::global_voice_assistant->on_event(msg);
  }
}
void APIConnection::on_voice_assistant_audio(const VoiceAssistantAudio &msg) {
  if (voice_assistant::global_voice_assistant != nullptr) {
    if (voice_assistant::global_voice_assistant->get_api_connection() != this) {
      return;
    }

    voice_assistant::global_voice_assistant->on_audio(msg);
  }
};
void APIConnection::on_voice_assistant_timer_event_response(const VoiceAssistantTimerEventResponse &msg) {
  if (voice_assistant::global_voice_assistant != nullptr) {
    if (voice_assistant::global_voice_assistant->get_api_connection() != this) {
      return;
    }

    voice_assistant::global_voice_assistant->on_timer_event(msg);
  }
};

void APIConnection::on_voice_assistant_announce_request(const VoiceAssistantAnnounceRequest &msg) {
  if (voice_assistant::global_voice_assistant != nullptr) {
    if (voice_assistant::global_voice_assistant->get_api_connection() != this) {
      return;
    }

    voice_assistant::global_voice_assistant->on_announce(msg);
  }
}

VoiceAssistantConfigurationResponse APIConnection::voice_assistant_get_configuration(
    const VoiceAssistantConfigurationRequest &msg) {
  VoiceAssistantConfigurationResponse resp;
  if (voice_assistant::global_voice_assistant != nullptr) {
    if (voice_assistant::global_voice_assistant->get_api_connection() != this) {
      return resp;
    }

    auto &config = voice_assistant::global_voice_assistant->get_configuration();
    for (auto &wake_word : config.available_wake_words) {
      VoiceAssistantWakeWord resp_wake_word;
      resp_wake_word.id = wake_word.id;
      resp_wake_word.wake_word = wake_word.wake_word;
      for (const auto &lang : wake_word.trained_languages) {
        resp_wake_word.trained_languages.push_back(lang);
      }
      resp.available_wake_words.push_back(std::move(resp_wake_word));
    }
    for (auto &wake_word_id : config.active_wake_words) {
      resp.active_wake_words.push_back(wake_word_id);
    }
    resp.max_active_wake_words = config.max_active_wake_words;
  }
  return resp;
}

void APIConnection::voice_assistant_set_configuration(const VoiceAssistantSetConfiguration &msg) {
  if (voice_assistant::global_voice_assistant != nullptr) {
    if (voice_assistant::global_voice_assistant->get_api_connection() != this) {
      return;
    }

    voice_assistant::global_voice_assistant->on_set_configuration(msg.active_wake_words);
  }
}

#endif

#ifdef USE_ALARM_CONTROL_PANEL
bool APIConnection::send_alarm_control_panel_state(alarm_control_panel::AlarmControlPanel *a_alarm_control_panel) {
  return this->schedule_message_(a_alarm_control_panel, &APIConnection::try_send_alarm_control_panel_state_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_alarm_control_panel_state_(EntityBase *entity) {
  auto *a_alarm_control_panel = static_cast<alarm_control_panel::AlarmControlPanel *>(entity);
  auto msg = std::make_unique<AlarmControlPanelStateResponse>();
  msg->state = static_cast<enums::AlarmControlPanelState>(a_alarm_control_panel->get_state());
  msg->key = a_alarm_control_panel->get_object_id_hash();
  return msg;
}
void APIConnection::send_alarm_control_panel_info(alarm_control_panel::AlarmControlPanel *a_alarm_control_panel) {
  this->schedule_message_(a_alarm_control_panel, &APIConnection::try_send_alarm_control_panel_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_alarm_control_panel_info_(EntityBase *entity) {
  auto *a_alarm_control_panel = static_cast<alarm_control_panel::AlarmControlPanel *>(entity);
  auto msg = std::make_unique<ListEntitiesAlarmControlPanelResponse>();
  msg->supported_features = a_alarm_control_panel->get_supported_features();
  msg->requires_code = a_alarm_control_panel->get_requires_code();
  msg->requires_code_to_arm = a_alarm_control_panel->get_requires_code_to_arm();
  msg->unique_id = get_default_unique_id("alarm_control_panel", a_alarm_control_panel);

  // Fill common entity fields
  fill_entity_info_base_(a_alarm_control_panel, *msg);

  return msg;
}
void APIConnection::alarm_control_panel_command(const AlarmControlPanelCommandRequest &msg) {
  alarm_control_panel::AlarmControlPanel *a_alarm_control_panel = App.get_alarm_control_panel_by_key(msg.key);
  if (a_alarm_control_panel == nullptr)
    return;

  auto call = a_alarm_control_panel->make_call();
  switch (msg.command) {
    case enums::ALARM_CONTROL_PANEL_DISARM:
      call.disarm();
      break;
    case enums::ALARM_CONTROL_PANEL_ARM_AWAY:
      call.arm_away();
      break;
    case enums::ALARM_CONTROL_PANEL_ARM_HOME:
      call.arm_home();
      break;
    case enums::ALARM_CONTROL_PANEL_ARM_NIGHT:
      call.arm_night();
      break;
    case enums::ALARM_CONTROL_PANEL_ARM_VACATION:
      call.arm_vacation();
      break;
    case enums::ALARM_CONTROL_PANEL_ARM_CUSTOM_BYPASS:
      call.arm_custom_bypass();
      break;
    case enums::ALARM_CONTROL_PANEL_TRIGGER:
      call.pending();
      break;
  }
  call.set_code(msg.code);
  call.perform();
}
#endif

#ifdef USE_EVENT
void APIConnection::send_event(event::Event *event, std::string event_type) {
  this->schedule_message_(event,
                          [event_type = std::move(event_type)](EntityBase *entity) -> std::unique_ptr<ProtoMessage> {
                            auto *e = static_cast<event::Event *>(entity);
                            auto msg = std::make_unique<EventResponse>();
                            msg->event_type = std::move(event_type);
                            msg->key = e->get_object_id_hash();
                            return msg;
                          });
}
void APIConnection::send_event_info(event::Event *event) {
  this->schedule_message_(event, &APIConnection::try_send_event_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_event_info_(EntityBase *entity) {
  auto *event = static_cast<event::Event *>(entity);
  auto msg = std::make_unique<ListEntitiesEventResponse>();
  msg->device_class = event->get_device_class();
  for (const auto &event_type : event->get_event_types())
    msg->event_types.push_back(event_type);
  msg->unique_id = get_default_unique_id("event", event);

  // Fill common entity fields
  fill_entity_info_base_(event, *msg);

  return msg;
}
#endif

#ifdef USE_UPDATE
bool APIConnection::send_update_state(update::UpdateEntity *update) {
  return this->schedule_message_(update, &APIConnection::try_send_update_state_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_update_state_(EntityBase *entity) {
  auto *update = static_cast<update::UpdateEntity *>(entity);
  auto msg = std::make_unique<UpdateStateResponse>();
  msg->missing_state = !update->has_state();
  if (update->has_state()) {
    msg->in_progress = update->state == update::UpdateState::UPDATE_STATE_INSTALLING;
    if (update->update_info.has_progress) {
      msg->has_progress = true;
      msg->progress = update->update_info.progress;
    }
    msg->current_version = update->update_info.current_version;
    msg->latest_version = update->update_info.latest_version;
    msg->title = update->update_info.title;
    msg->release_summary = update->update_info.summary;
    msg->release_url = update->update_info.release_url;
  }
  msg->key = update->get_object_id_hash();
  return msg;
}
void APIConnection::send_update_info(update::UpdateEntity *update) {
  this->schedule_message_(update, &APIConnection::try_send_update_info_);
}
std::unique_ptr<ProtoMessage> APIConnection::try_send_update_info_(EntityBase *entity) {
  auto *update = static_cast<update::UpdateEntity *>(entity);
  auto msg = std::make_unique<ListEntitiesUpdateResponse>();
  msg->device_class = update->get_device_class();
  msg->unique_id = get_default_unique_id("update", update);

  // Fill common entity fields
  fill_entity_info_base_(update, *msg);

  return msg;
}
void APIConnection::update_command(const UpdateCommandRequest &msg) {
  update::UpdateEntity *update = App.get_update_by_key(msg.key);
  if (update == nullptr)
    return;

  switch (msg.command) {
    case enums::UPDATE_COMMAND_UPDATE:
      update->perform();
      break;
    case enums::UPDATE_COMMAND_CHECK:
      update->check();
      break;
    case enums::UPDATE_COMMAND_NONE:
      ESP_LOGE(TAG, "UPDATE_COMMAND_NONE not handled; confirm command is correct");
      break;
    default:
      ESP_LOGW(TAG, "Unknown update command: %" PRIu32, msg.command);
      break;
  }
}
#endif

bool APIConnection::try_send_log_message(int level, const char *tag, const char *line) {
  if (this->log_subscription_ < level)
    return false;

  // Pre-calculate message size to avoid reallocations
  const size_t line_length = strlen(line);
  uint32_t msg_size = 0;

  // Add size for level field (field ID 1, varint type)
  // 1 byte for field tag + size of the level varint
  msg_size += 1 + api::ProtoSize::varint(static_cast<uint32_t>(level));

  // Add size for string field (field ID 3, string type)
  // 1 byte for field tag + size of length varint + string length
  msg_size += 1 + api::ProtoSize::varint(static_cast<uint32_t>(line_length)) + line_length;

  // Create a pre-sized buffer
  auto buffer = this->create_buffer(msg_size);

  // Encode the message (SubscribeLogsResponse)
  buffer.encode_uint32(1, static_cast<uint32_t>(level));  // LogLevel level = 1
  buffer.encode_string(3, line, line_length);             // string message = 3

  // SubscribeLogsResponse - 29
  return this->send_buffer(buffer, 29);
}

HelloResponse APIConnection::hello(const HelloRequest &msg) {
  this->client_info_ = msg.client_info;
  this->client_peername_ = this->helper_->getpeername();
  this->client_combined_info_ = this->client_info_ + " (" + this->client_peername_ + ")";
  this->helper_->set_log_info(this->client_combined_info_);
  this->client_api_version_major_ = msg.api_version_major;
  this->client_api_version_minor_ = msg.api_version_minor;
  ESP_LOGV(TAG, "Hello from client: '%s' | %s | API Version %" PRIu32 ".%" PRIu32, this->client_info_.c_str(),
           this->client_peername_.c_str(), this->client_api_version_major_, this->client_api_version_minor_);

  HelloResponse resp;
  resp.api_version_major = 1;
  resp.api_version_minor = 10;
  resp.server_info = App.get_name() + " (esphome v" ESPHOME_VERSION ")";
  resp.name = App.get_name();

  this->connection_state_ = ConnectionState::CONNECTED;
  return resp;
}
ConnectResponse APIConnection::connect(const ConnectRequest &msg) {
  bool correct = this->parent_->check_password(msg.password);

  ConnectResponse resp;
  // bool invalid_password = 1;
  resp.invalid_password = !correct;
  if (correct) {
    ESP_LOGD(TAG, "%s connected", this->client_combined_info_.c_str());
    this->connection_state_ = ConnectionState::AUTHENTICATED;
    this->parent_->get_client_connected_trigger()->trigger(this->client_info_, this->client_peername_);
#ifdef USE_HOMEASSISTANT_TIME
    if (homeassistant::global_homeassistant_time != nullptr) {
      this->send_time_request();
    }
#endif
  }
  return resp;
}
DeviceInfoResponse APIConnection::device_info(const DeviceInfoRequest &msg) {
  DeviceInfoResponse resp{};
  resp.uses_password = this->parent_->uses_password();
  resp.name = App.get_name();
  resp.friendly_name = App.get_friendly_name();
  resp.suggested_area = App.get_area();
  resp.mac_address = get_mac_address_pretty();
  resp.esphome_version = ESPHOME_VERSION;
  resp.compilation_time = App.get_compilation_time();
#if defined(USE_ESP8266) || defined(USE_ESP32)
  resp.manufacturer = "Espressif";
#elif defined(USE_RP2040)
  resp.manufacturer = "Raspberry Pi";
#elif defined(USE_BK72XX)
  resp.manufacturer = "Beken";
#elif defined(USE_RTL87XX)
  resp.manufacturer = "Realtek";
#elif defined(USE_HOST)
  resp.manufacturer = "Host";
#endif
  resp.model = ESPHOME_BOARD;
#ifdef USE_DEEP_SLEEP
  resp.has_deep_sleep = deep_sleep::global_has_deep_sleep;
#endif
#ifdef ESPHOME_PROJECT_NAME
  resp.project_name = ESPHOME_PROJECT_NAME;
  resp.project_version = ESPHOME_PROJECT_VERSION;
#endif
#ifdef USE_WEBSERVER
  resp.webserver_port = USE_WEBSERVER_PORT;
#endif
#ifdef USE_BLUETOOTH_PROXY
  resp.legacy_bluetooth_proxy_version = bluetooth_proxy::global_bluetooth_proxy->get_legacy_version();
  resp.bluetooth_proxy_feature_flags = bluetooth_proxy::global_bluetooth_proxy->get_feature_flags();
  resp.bluetooth_mac_address = bluetooth_proxy::global_bluetooth_proxy->get_bluetooth_mac_address_pretty();
#endif
#ifdef USE_VOICE_ASSISTANT
  resp.legacy_voice_assistant_version = voice_assistant::global_voice_assistant->get_legacy_version();
  resp.voice_assistant_feature_flags = voice_assistant::global_voice_assistant->get_feature_flags();
#endif
#ifdef USE_API_NOISE
  resp.api_encryption_supported = true;
#endif
  return resp;
}
void APIConnection::on_home_assistant_state_response(const HomeAssistantStateResponse &msg) {
  for (auto &it : this->parent_->get_state_subs()) {
    if (it.entity_id == msg.entity_id && it.attribute.value() == msg.attribute) {
      it.callback(msg.state);
    }
  }
}
void APIConnection::execute_service(const ExecuteServiceRequest &msg) {
  bool found = false;
  for (auto *service : this->parent_->get_user_services()) {
    if (service->execute_service(msg)) {
      found = true;
    }
  }
  if (!found) {
    ESP_LOGV(TAG, "Could not find service");
  }
}
#ifdef USE_API_NOISE
NoiseEncryptionSetKeyResponse APIConnection::noise_encryption_set_key(const NoiseEncryptionSetKeyRequest &msg) {
  psk_t psk{};
  NoiseEncryptionSetKeyResponse resp;
  if (base64_decode(msg.key, psk.data(), msg.key.size()) != psk.size()) {
    ESP_LOGW(TAG, "Invalid encryption key length");
    resp.success = false;
    return resp;
  }

  if (!this->parent_->save_noise_psk(psk, true)) {
    ESP_LOGW(TAG, "Failed to save encryption key");
    resp.success = false;
    return resp;
  }

  resp.success = true;
  return resp;
}
#endif
void APIConnection::subscribe_home_assistant_states(const SubscribeHomeAssistantStatesRequest &msg) {
  state_subs_at_ = 0;
}
bool APIConnection::try_to_clear_buffer(bool log_out_of_space) {
  if (this->remove_)
    return false;
  if (this->helper_->can_write_without_blocking())
    return true;
  delay(0);
  APIError err = this->helper_->loop();
  if (err != APIError::OK) {
    on_fatal_error();
    ESP_LOGW(TAG, "%s: Socket operation failed: %s errno=%d", this->client_combined_info_.c_str(),
             api_error_to_str(err), errno);
    return false;
  }
  if (this->helper_->can_write_without_blocking())
    return true;
  if (log_out_of_space) {
    ESP_LOGV(TAG, "Cannot send message because of TCP buffer space");
  }
  return false;
}
bool APIConnection::send_buffer(ProtoWriteBuffer buffer, uint16_t message_type) {
  if (!this->try_to_clear_buffer(message_type != 29)) {  // SubscribeLogsResponse
    return false;
  }

  APIError err = this->helper_->write_protobuf_packet(message_type, buffer);
  if (err == APIError::WOULD_BLOCK)
    return false;
  if (err != APIError::OK) {
    on_fatal_error();
    if (err == APIError::SOCKET_WRITE_FAILED && errno == ECONNRESET) {
      ESP_LOGW(TAG, "%s: Connection reset", this->client_combined_info_.c_str());
    } else {
      ESP_LOGW(TAG, "%s: Packet write failed %s errno=%d", this->client_combined_info_.c_str(), api_error_to_str(err),
               errno);
    }
    return false;
  }
  // Do not set last_traffic_ on send
  return true;
}
void APIConnection::on_unauthenticated_access() {
  this->on_fatal_error();
  ESP_LOGD(TAG, "%s requested access without authentication", this->client_combined_info_.c_str());
}
void APIConnection::on_no_setup_connection() {
  this->on_fatal_error();
  ESP_LOGD(TAG, "%s requested access without full connection", this->client_combined_info_.c_str());
}
void APIConnection::on_fatal_error() {
  this->helper_->close();
  this->remove_ = true;
}

void APIConnection::DeferredBatch::add_item(EntityBase *entity, MessageCreator creator) {
  // Add new item without deduplication for now
  items.push_back({entity, std::move(creator), App.get_loop_component_start_time()});
}

bool APIConnection::schedule_batch_() {
  if (!this->deferred_batch_.batch_scheduled) {
    this->deferred_batch_.batch_scheduled = true;
    this->deferred_batch_.batch_start_time = App.get_loop_component_start_time();
  }
  return true;
}

void APIConnection::process_batch_() {
  if (this->deferred_batch_.empty()) {
    this->deferred_batch_.batch_scheduled = false;
    return;
  }

  ESP_LOGD(TAG, "Processing batch with %zu items", this->deferred_batch_.items.size());

  // Try to clear buffer first
  if (!this->helper_->can_write_without_blocking()) {
    // Can't write now, we'll try again later
    return;
  }

  // First pass: create messages and calculate sizes
  struct MessageInfo {
    std::unique_ptr<ProtoMessage> message;
    uint16_t message_size;
  };
  std::vector<MessageInfo> messages;
  messages.reserve(this->deferred_batch_.items.size());

  for (const auto &item : this->deferred_batch_.items) {
    // Call the creator function with the entity to generate the message
    auto message = item.creator(item.entity);
    if (!message) {
      continue;  // Skip if creator returned nullptr
    }

    // Calculate message size
    uint32_t size = 0;
    message->calculate_size(size);
    uint16_t message_size = static_cast<uint16_t>(size);

    messages.push_back({std::move(message), message_size});
  }

  if (messages.empty()) {
    this->deferred_batch_.clear();
    return;
  }

  // Calculate the total size needed and build packet info
  std::vector<std::tuple<uint16_t, uint32_t, uint16_t>> packet_info;
  packet_info.reserve(messages.size());

  uint32_t total_buffer_size = 0;
  size_t messages_to_process = 0;
  uint32_t current_offset = 0;  // Track offset for each packet

  for (size_t i = 0; i < messages.size(); i++) {
    const auto &msg_info = messages[i];
    uint16_t message_type = msg_info.message->get_message_type();
    uint16_t overhead = this->helper_->calculate_packet_overhead(message_type, msg_info.message_size);
    uint32_t packet_size = msg_info.message_size + overhead;

    // Check if adding this message would exceed our limit
    if (messages_to_process > 0 && total_buffer_size + packet_size > MAX_BATCH_SIZE_BYTES) {
      break;  // This and remaining messages won't fit
    }

    // Add to packet info (type, offset where packet starts, message length)
    // For the first message, offset is 0; for subsequent messages, it's after previous message + padding
    packet_info.push_back(std::make_tuple(message_type, current_offset, msg_info.message_size));

    // Update offset for next message: add header padding for next message if not last
    if (i < messages.size() - 1) {
      current_offset += msg_info.message_size + this->helper_->frame_header_padding();
    }

    total_buffer_size += packet_size;
    messages_to_process++;
  }

  ProtoWriteBuffer batch_buffer = this->create_buffer(total_buffer_size);

  // Encode all messages
  for (size_t i = 0; i < messages_to_process; i++) {
    const auto &msg_info = messages[i];

    // For messages after the first, extend the buffer with padding
    if (i > 0) {
      batch_buffer = this->extend_buffer();
    }

    // Encode the message
    msg_info.message->encode(batch_buffer);
  }

  // Send all collected packets
  if (!packet_info.empty()) {
    // Add final footer space for Noise if needed
    if (this->helper_->frame_footer_size() > 0) {
      this->proto_write_buffer_.resize(this->proto_write_buffer_.size() + this->helper_->frame_footer_size());
    }

    ESP_LOGD(TAG, "Sending batch: %zu messages, %zu total bytes", messages_to_process,
             this->proto_write_buffer_.size());

    APIError err = this->helper_->write_protobuf_packets(batch_buffer, packet_info);
    if (err != APIError::OK && err != APIError::WOULD_BLOCK) {
      on_fatal_error();
      if (err == APIError::SOCKET_WRITE_FAILED && errno == ECONNRESET) {
        ESP_LOGW(TAG, "%s: Connection reset during batch write", this->client_combined_info_.c_str());
      } else {
        ESP_LOGW(TAG, "%s: Batch write failed %s errno=%d", this->client_combined_info_.c_str(), api_error_to_str(err),
                 errno);
      }
    }
  }

  // Remove processed items from the batch
  if (messages_to_process < this->deferred_batch_.items.size()) {
    // Some items weren't processed, keep them for next batch
    this->deferred_batch_.items.erase(this->deferred_batch_.items.begin(),
                                      this->deferred_batch_.items.begin() + messages_to_process);
    // Reschedule for remaining items
    this->schedule_batch_();
  } else {
    // All items processed
    this->deferred_batch_.clear();
  }
}

}  // namespace api
}  // namespace esphome
#endif
