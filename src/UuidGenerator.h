#pragma once

#include <string>

namespace Dawn {

// Generates a random RFC-4122 version-4 UUID (e.g. "a1b2c3d4-e5f6-7890-abcd-ef1234567890").
// Uses only the standard library <random>; no external dependency. Pure logic — no
// SDL, Penumbra, or Firefly — so it compiles into both dawn and dawn_tests.
std::string GenerateUuidV4();

} // namespace Dawn
