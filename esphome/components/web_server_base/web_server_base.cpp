#include "web_server_base.h"
#ifdef USE_NETWORK
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_WEBSERVER_OTA
#include "esphome/components/ota_base/ota_backend.h"
#endif

#ifdef USE_ARDUINO
#ifdef USE_ESP8266
#include <Updater.h>
#elif defined(USE_ESP32) || defined(USE_LIBRETINY)
#include <Update.h>
#endif
#endif

namespace esphome {
namespace web_server_base {

static const char *const TAG = "web_server_base";

void WebServerBase::add_handler(AsyncWebHandler *handler) {
  // remove all handlers

  if (!credentials_.username.empty()) {
    handler = new internal::AuthMiddlewareHandler(handler, &credentials_);
  }
  this->handlers_.push_back(handler);
  if (this->server_ != nullptr) {
    this->server_->addHandler(handler);
  }
}

#ifdef USE_WEBSERVER_OTA
void OTARequestHandler::report_ota_progress_(AsyncWebServerRequest *request) {
  const uint32_t now = millis();
  if (now - this->last_ota_progress_ > 1000) {
    float percentage = 0.0f;
    if (request->contentLength() != 0) {
      // Note: Using contentLength() for progress calculation is technically wrong as it includes
      // multipart headers/boundaries, but it's only off by a small amount and we don't have
      // access to the actual firmware size until the upload is complete. This is intentional
      // as it still gives the user a reasonable progress indication.
      percentage = (this->ota_read_length_ * 100.0f) / request->contentLength();
      ESP_LOGD(TAG, "OTA in progress: %0.1f%%", percentage);
    } else {
      ESP_LOGD(TAG, "OTA in progress: %u bytes read", this->ota_read_length_);
    }
#ifdef USE_OTA_STATE_CALLBACK
    // Report progress - use call_deferred since we're in web server task
    this->parent_->state_callback_.call_deferred(ota_base::OTA_IN_PROGRESS, percentage, 0);
#endif
    this->last_ota_progress_ = now;
  }
}

void OTARequestHandler::schedule_ota_reboot_() {
  ESP_LOGI(TAG, "OTA update successful!");
  this->parent_->set_timeout(100, []() {
    ESP_LOGI(TAG, "Performing OTA reboot now");
    App.safe_reboot();
  });
}

void OTARequestHandler::ota_init_(const char *filename) {
  ESP_LOGI(TAG, "OTA Update Start: %s", filename);
  this->ota_read_length_ = 0;
}

void OTARequestHandler::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index,
                                     uint8_t *data, size_t len, bool final) {
  ota_base::OTAResponseTypes error_code = ota_base::OTA_RESPONSE_OK;

  if (index == 0 && !this->ota_backend_) {
    // Initialize OTA on first call
    this->ota_init_(filename.c_str());

#ifdef USE_OTA_STATE_CALLBACK
    // Notify OTA started - use call_deferred since we're in web server task
    this->parent_->state_callback_.call_deferred(ota_base::OTA_STARTED, 0.0f, 0);
#endif

    // Platform-specific pre-initialization
#ifdef USE_ARDUINO
#ifdef USE_ESP8266
    Update.runAsync(true);
#endif
#if defined(USE_ESP32_FRAMEWORK_ARDUINO) || defined(USE_LIBRETINY)
    if (Update.isRunning()) {
      Update.abort();
    }
#endif
#endif  // USE_ARDUINO

    this->ota_backend_ = ota_base::make_ota_backend();
    if (!this->ota_backend_) {
      ESP_LOGE(TAG, "Failed to create OTA backend");
#ifdef USE_OTA_STATE_CALLBACK
      this->parent_->state_callback_.call_deferred(ota_base::OTA_ERROR, 0.0f,
                                                   static_cast<uint8_t>(ota_base::OTA_RESPONSE_ERROR_UNKNOWN));
#endif
      return;
    }

    // Web server OTA uses multipart uploads where the actual firmware size
    // is unknown (contentLength includes multipart overhead)
    // Pass 0 to indicate unknown size
    error_code = this->ota_backend_->begin(0);
    if (error_code != ota_base::OTA_RESPONSE_OK) {
      ESP_LOGE(TAG, "OTA begin failed: %d", error_code);
      this->ota_backend_.reset();
#ifdef USE_OTA_STATE_CALLBACK
      this->parent_->state_callback_.call_deferred(ota_base::OTA_ERROR, 0.0f, static_cast<uint8_t>(error_code));
#endif
      return;
    }
  }

  if (!this->ota_backend_) {
    return;
  }

  // Process data
  if (len > 0) {
    error_code = this->ota_backend_->write(data, len);
    if (error_code != ota_base::OTA_RESPONSE_OK) {
      ESP_LOGE(TAG, "OTA write failed: %d", error_code);
      this->ota_backend_->abort();
      this->ota_backend_.reset();
#ifdef USE_OTA_STATE_CALLBACK
      this->parent_->state_callback_.call_deferred(ota_base::OTA_ERROR, 0.0f, static_cast<uint8_t>(error_code));
#endif
      return;
    }
    this->ota_read_length_ += len;
    this->report_ota_progress_(request);
  }

  // Finalize
  if (final) {
    error_code = this->ota_backend_->end();
    if (error_code == ota_base::OTA_RESPONSE_OK) {
#ifdef USE_OTA_STATE_CALLBACK
      // Report completion before reboot - use call_deferred since we're in web server task
      this->parent_->state_callback_.call_deferred(ota_base::OTA_COMPLETED, 100.0f, 0);
#endif
      this->schedule_ota_reboot_();
    } else {
      ESP_LOGE(TAG, "OTA end failed: %d", error_code);
#ifdef USE_OTA_STATE_CALLBACK
      this->parent_->state_callback_.call_deferred(ota_base::OTA_ERROR, 0.0f, static_cast<uint8_t>(error_code));
#endif
    }
    this->ota_backend_.reset();
  }
}

void OTARequestHandler::handleRequest(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response;
  // Send response based on whether backend still exists (error) or was reset (success)
  response = request->beginResponse(200, "text/plain", !this->ota_backend_ ? "Update Successful!" : "Update Failed!");
  response->addHeader("Connection", "close");
  request->send(response);
}

void WebServerBase::add_ota_handler() {
  this->add_handler(new OTARequestHandler(this));  // NOLINT
#ifdef USE_OTA_STATE_CALLBACK
  // Register with global OTA callback system
  ota_base::register_ota_platform(this);
#endif
}
#endif

float WebServerBase::get_setup_priority() const {
  // Before WiFi (captive portal)
  return setup_priority::WIFI + 2.0f;
}

}  // namespace web_server_base
}  // namespace esphome
#endif
