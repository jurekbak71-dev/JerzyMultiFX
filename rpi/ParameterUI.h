#pragma once
#include "core/ControlUI.h"
namespace multifx::rpi {
using multifx::ParameterUI;
inline const std::vector<ParameterUI> &parameterUI() { return multifx::controlUI(); }
inline float discreteValue(int id, float value) { return multifx::discreteValue(id, value); }
} // namespace multifx::rpi
