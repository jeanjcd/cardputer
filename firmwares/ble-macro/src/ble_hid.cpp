#include "ble_hid.h"

#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <HIDTypes.h>

static const uint8_t kReportMap[] = {
  // ---- Keyboard, report ID 1 ----
  USAGE_PAGE(1), 0x01,        // Generic Desktop
  USAGE(1), 0x06,             // Keyboard
  COLLECTION(1), 0x01,        // Application
  REPORT_ID(1), 0x01,
  USAGE_PAGE(1), 0x07,        //   Keyboard/Keypad
  USAGE_MINIMUM(1), 0xE0,
  USAGE_MAXIMUM(1), 0xE7,
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x01,
  REPORT_SIZE(1), 0x01,
  REPORT_COUNT(1), 0x08,
  HIDINPUT(1), 0x02,          //   modifiers: Data,Var,Abs
  REPORT_COUNT(1), 0x01,
  REPORT_SIZE(1), 0x08,
  HIDINPUT(1), 0x01,          //   reserved: Const
  REPORT_COUNT(1), 0x06,
  REPORT_SIZE(1), 0x08,
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x65,
  USAGE_PAGE(1), 0x07,
  USAGE_MINIMUM(1), 0x00,
  USAGE_MAXIMUM(1), 0x65,
  HIDINPUT(1), 0x00,          //   6 keys: Data,Array,Abs
  END_COLLECTION(0),

  // ---- Mouse, report ID 2 ----
  USAGE_PAGE(1), 0x01,
  USAGE(1), 0x02,             // Mouse
  COLLECTION(1), 0x01,
  REPORT_ID(1), 0x02,
  USAGE(1), 0x01,             //   Pointer
  COLLECTION(1), 0x00,        //   Physical
  USAGE_PAGE(1), 0x09,        //     Buttons
  USAGE_MINIMUM(1), 0x01,
  USAGE_MAXIMUM(1), 0x03,
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x01,
  REPORT_COUNT(1), 0x03,
  REPORT_SIZE(1), 0x01,
  HIDINPUT(1), 0x02,
  REPORT_COUNT(1), 0x01,
  REPORT_SIZE(1), 0x05,
  HIDINPUT(1), 0x01,          //     padding
  USAGE_PAGE(1), 0x01,
  USAGE(1), 0x30,             //     X
  USAGE(1), 0x31,             //     Y
  USAGE(1), 0x38,             //     Wheel
  LOGICAL_MINIMUM(1), 0x81,   //     -127
  LOGICAL_MAXIMUM(1), 0x7F,   //     127
  REPORT_SIZE(1), 0x08,
  REPORT_COUNT(1), 0x03,
  HIDINPUT(1), 0x06,          //     Data,Var,Rel
  END_COLLECTION(0),
  END_COLLECTION(0),
};

static volatile bool g_connected = false;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override { g_connected = true; }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    g_connected = false;
    NimBLEDevice::startAdvertising();
  }
};

void BleHid::begin(const char* name) {
  NimBLEDevice::init(name);
  NimBLEDevice::setSecurityAuth(true, false, false);  // bonding, no MITM, no SC

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  hid_ = new NimBLEHIDDevice(server);
  keyboard_ = hid_->getInputReport(1);
  mouse_ = hid_->getInputReport(2);

  hid_->setManufacturer("M5Stack");
  hid_->setPnp(0x02, 0xe502, 0xa111, 0x0210);
  hid_->setHidInfo(0x00, 0x02);
  hid_->setReportMap((uint8_t*)kReportMap, sizeof(kReportMap));
  hid_->startServices();
  hid_->setBatteryLevel(100);

  NimBLEAdvertising* adv = server->getAdvertising();
  adv->setAppearance(HID_KEYBOARD);
  adv->addServiceUUID(hid_->getHidService()->getUUID());
  adv->enableScanResponse(true);
  adv->start();
}

bool BleHid::isConnected() const { return g_connected; }

void BleHid::mouseMove(int8_t dx, int8_t dy) {
  if (!g_connected) return;
  uint8_t r[4] = {0, (uint8_t)dx, (uint8_t)dy, 0};
  mouse_->setValue(r, sizeof(r));
  mouse_->notify();
}

void BleHid::keyDown(uint8_t modifier) {
  if (!g_connected) return;
  uint8_t r[8] = {modifier, 0, 0, 0, 0, 0, 0, 0};
  keyboard_->setValue(r, sizeof(r));
  keyboard_->notify();
}

void BleHid::releaseAll() {
  if (!g_connected) return;
  uint8_t k[8] = {0};
  keyboard_->setValue(k, sizeof(k));
  keyboard_->notify();
  uint8_t m[4] = {0};
  mouse_->setValue(m, sizeof(m));
  mouse_->notify();
}
