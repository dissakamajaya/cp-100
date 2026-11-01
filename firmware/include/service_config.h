#pragma once
// Shared operator hotspot settings explicitly selected by the owner.
// Changing this password in a future build replaces the old NVS-generated password.
constexpr char kServiceSsid[] = "ATMOS-CP-300-EXP";
constexpr char kServicePassword[] = "300300300";
static_assert(sizeof(kServicePassword) >= 9 && sizeof(kServicePassword) <= 64,
              "Hotspot passphrase must contain 8-63 characters");
