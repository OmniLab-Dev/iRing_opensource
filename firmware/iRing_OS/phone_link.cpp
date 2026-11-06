#include <bluefruit.h>
#include "class/hid/hid_device.h"
#include "phone_link.h"
namespace {
// Only Consumer Control: no keyboard or mouse, so Android's on-screen keyboard
// remains available. Reports are a 16-bit usage; zero releases every key.
const uint8_t reportMap[] = {TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(1))};
class MediaHid : public BLEHidGeneric {
public:
  MediaHid() : BLEHidGeneric(1) {}
  err_t begin() override {
    static uint16_t lengths[] = {2};
    setReportLen(lengths);
    setReportMap(reportMap, sizeof reportMap);
    return BLEHidGeneric::begin();
  }
  bool subscribed(uint16_t handle) {
    return _chr_inputs[0].notifyEnabled(handle);
  }
} hid;
MediaQueue queue;
volatile uint16_t connection = BLE_CONN_HANDLE_INVALID;
volatile uint32_t generation = 0;
uint32_t seenGeneration = 0;
bool started = false;
void connected(uint16_t handle) {
  connection = handle;
  ++generation;
  BLEConnection *peer = Bluefruit.Connection(handle);
  if (peer && !peer->secured())
    peer->requestPairing();
}
void disconnected(uint16_t, uint8_t) {
  connection = BLE_CONN_HANDLE_INVALID;
  ++generation;
}
} // namespace
bool phoneBegin() {
  started = hid.begin() == ERROR_NONE;
  Bluefruit.Periph.setConnInterval(12, 24); // 15-30 ms; accepted by Android HID hosts.
  Bluefruit.Periph.setConnectCallback(connected);
  Bluefruit.Periph.setDisconnectCallback(disconnected);
  if (started) {
    Bluefruit.setAppearance(BLE_APPEARANCE_GENERIC_HID);
    Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_GENERIC_HID);
    Bluefruit.Advertising.addService(hid);
  }
  return started;
}
PhoneStatus phoneStatus() {
  uint16_t handle = connection;
  BLEConnection *peer =
      handle == BLE_CONN_HANDLE_INVALID ? nullptr : Bluefruit.Connection(handle);
  if (!peer || !peer->connected())
    return PHONE_DISCONNECTED;
  return started && peer->secured() && hid.subscribed(handle) ? PHONE_READY
                                                              : PHONE_PAIRING;
}
uint32_t phoneGeneration() {
  return generation;
}
bool phoneSend(MediaKey key) {
  if (seenGeneration != generation || phoneStatus() != PHONE_READY)
    return false;
  return queue.push(key, millis());
}
uint16_t phonePoll(uint32_t now) {
  if (seenGeneration != generation) {
    queue.reset();
    seenGeneration = generation;
  }
  uint16_t handle = connection;
  return queue.tick(now, phoneStatus() == PHONE_READY, [handle](uint16_t key) {
    return hid.inputReport(handle, 1, &key, sizeof key);
  });
}
