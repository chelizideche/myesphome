#pragma once

#include "esphome/core/component.h"

namespace esphome {
namespace energy_sampler {

/// @brief Enum representing the three phases in a multi-phase energy monitoring system.
enum class Phase : uint8_t {
  PHASE_1 = 0,  ///< Phase 1 (index 0)
  PHASE_2 = 1,  ///< Phase 2 (index 1)
  PHASE_3 = 2   ///< Phase 3 (index 2)
};

/// @brief Struct to hold sampled parameters for a single phase.
/// @details Contains the instantaneous voltage and current readings for a specific phase.
struct PhaseSampleData {
  float voltage;  ///< Instantaneous voltage for the phase, in volts (V).
  float current;  ///< Instantaneous current for the phase, in amperes (A).
};

/// @brief Struct to hold sampled parameters for all three phases and the neutral current.
/// @details Includes the voltage and current for each phase and the neutral current for the system.
struct EnergySampleData {
  PhaseSampleData phases[3];  ///< Array holding data for all 3 phases.
  float neutral_current;      ///< Instantaneous neutral current for the system, in amperes (A).
};

/// @brief Abstract interface for components to request instantaneous voltage and current readings from energy meters.
class EnergySampler {
 public:
  /// @brief Get the instantaneous voltage for a specific phase.
  /// @param[in] phase The phase for which to retrieve the voltage (PHASE_1, PHASE_2, or PHASE_3).
  /// @return The instantaneous voltage for the specified phase, in volts (V).
  virtual float get_instantaneous_voltage(const Phase phase) = 0;

  /// @brief Get the instantaneous current for a specific phase.
  /// @param[in] phase The phase for which to retrieve the current (PHASE_1, PHASE_2, or PHASE_3).
  /// @return The instantaneous current for the specified phase, in amperes (A).
  virtual float get_instantaneous_current(const Phase phase) = 0;

  /// @brief Get the instantaneous neutral current for the multi-phase system.
  /// @details The neutral current is the current flowing through the neutral conductor in the system,
  ///          not related to any individual phase.
  /// @return The instantaneous neutral current, in amperes (A).
  virtual float get_instantaneous_neutral_current() = 0;

  /// @brief Get sampled parameters (voltage and current) for a specific phase.
  /// @param[in] phase The phase for which to retrieve the parameters (PHASE_1, PHASE_2, or PHASE_3).
  /// @param[out] data Pointer to a PhaseSampleData struct to store the retrieved parameters.
  virtual void get_instantaneous_parameters_for_phase(const Phase phase, PhaseSampleData *data) = 0;

  /// @brief Get sampled parameters for all phases and the neutral current.
  /// @param[out] data Pointer to an EnergySampleData struct to store the retrieved parameters.
  virtual void get_instantaneous_parameters_for_all_phases(EnergySampleData *data) = 0;
};

}  // namespace energy_sampler
}  // namespace esphome
