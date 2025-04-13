#include "LD2410SButton.h"

namespace esphome {
namespace ld2410s {

void LD2410SReadAll::press_action() { this->parent_->read_all(); }

void LD2410SApplyConfigButton::press_action() { this->parent_->write_all(); }

void LD2410SCalibration::press_action() { this->parent_->calibration(); }

void LD2410SFactoryReset::press_action() { this->parent_->factory_reset(); }

void LD2410SMinimalOutput::press_action() { this->parent_->toggle_minimal(); }

}  // namespace ld2410s
}  // namespace esphome
