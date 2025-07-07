#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"
#ifndef USE_RP2040
#include "esphome/components/web_server_base/web_server_base.h"
#endif
#include <memory>
#include "decompressor.h"

extern const uint8_t ESPHOME_YAML[] PROGMEM;
extern const size_t ESPHOME_YAML_SIZE;

namespace esphome {
namespace store_yaml {

#ifndef USE_RP2040
class StoreYamlComponent : public Component,
                           public AsyncWebHandler
#else
class StoreYamlComponent : public Component
#endif
{
  std::unique_ptr<RowDecompressor> dec_;
  bool show_in_dump_config_{false};

#ifndef USE_RP2040
  web_server_base::WebServerBase *web_server_;
  std::string web_server_url_;
  std::unique_ptr<Decompressor> web_dec_;
  bool canHandle(AsyncWebServerRequest *request) override;
  void handleRequest(AsyncWebServerRequest *request) override;
#endif

 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::WIFI - 1.0f; }
  void setup() override;
  void loop() override;
  void log();
  void set_show_in_dump_config(bool show) { this->show_in_dump_config_ = show; }
#ifndef USE_RP2040
  void set_web_server(web_server_base::WebServerBase *web_server, const std::string &url);
#endif
};

template<typename... Ts> class LogAction : public Action<Ts...>, public Parented<StoreYamlComponent> {
 public:
  void play(Ts... x) override { this->parent_->log(); }
};

}  // namespace store_yaml
}  // namespace esphome
