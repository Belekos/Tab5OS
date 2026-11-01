#pragma once
#include "Tab5MeshModel.h"
namespace Tab5MeshStore {
// Small settings/address book in NVS. Never contains a PIN or BLE session keys.
bool loadPreferences(Tab5MeshModel::Preferences&);
bool savePreferences(const Tab5MeshModel::Preferences&);
// SD history alternates two checksummed generations. A failed write preserves
// the previous generation. Called only by a short-lived IO worker, never LVGL.
bool loadHistory(Tab5MeshModel::History&);
bool saveHistory(const Tab5MeshModel::History&);
}
