#include "LD2410SButton.h"

namespace esphome {
namespace ld2410s {

void LD2410SReadAll::press_action() { this->parent_->read_all_(); }

void LD2410SApplyConfigButton::press_action() { this->parent_->apply_config(); }

void LD2410SCalibration::press_action() { this->parent_->calibration(); }

void LD2410SFactoryReset::press_action() { this->parent_->factory_reset(); }

void LD2410SMinimalOutput::press_action() { this->parent_->toggle_minimal(); }

// void LD2410SDisableConfigButton::press_action()
// {
//     this->parent_->disable_configuration_command();
// }
// void LD2410SEnableConfigButton::press_action()
// {
//     this->parent_->enable_configuration_command();
// }

}  // namespace ld2410s
}  // namespace esphome
