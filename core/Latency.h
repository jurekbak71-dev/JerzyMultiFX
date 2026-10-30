#pragma once
namespace multifx {
inline constexpr int attackLatency = 2064; // POLY spectral overlap-add: 2048 + 16.
inline constexpr int attackMonoLatency = 16; // MONO does not need the POLY analysis window.
inline constexpr int atomicLatency = 2048;
inline constexpr int modulationLatency = attackLatency + atomicLatency;
inline constexpr int convolutionLatency = 128;
inline constexpr int driveLatency = 16;
inline constexpr int chainLatency = driveLatency + modulationLatency + convolutionLatency;
} // namespace multifx
