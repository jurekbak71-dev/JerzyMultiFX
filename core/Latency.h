#pragma once
namespace multifx {
inline constexpr int attackLatency = 2064; // Original spectral/mono alignment: 2048 + 16.
inline constexpr int atomicLatency = 2048;
inline constexpr int modulationLatency = attackLatency + atomicLatency;
inline constexpr int convolutionLatency = 128;
inline constexpr int driveLatency = 16;
inline constexpr int chainLatency = driveLatency + modulationLatency + convolutionLatency;
} // namespace multifx
