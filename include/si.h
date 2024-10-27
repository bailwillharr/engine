#pragma once

/* all units in this engine are in SI (meters, kilograms, seconds) unless stated otherwise */

namespace engine {

constexpr inline float inchesToMeters(float inches) { return inches * 25.4f / 1000.0f; }
constexpr inline float feetToMeters(float feet) { return inchesToMeters(feet * 12.0f); }
constexpr inline float yardsToMeters(float yards) { return inchesToMeters(yards * 36.0f); }

constexpr inline float metersToInches(float meters) { return meters * 1000.0f / 25.4f; }
constexpr inline float metersToFeet(float meters) { return metersToInches(meters) / 12.0f; }
constexpr inline float metersToYards(float meters) { return metersToInches(meters) / 36.0f; }

} // namespace engine
