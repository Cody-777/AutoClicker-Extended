#include "AutoClicker.h"

AutoClicker::AutoClicker() : running(false), clickCount(0) {}

AutoClicker::~AutoClicker() { Stop(); }

void AutoClicker::Start(const ClickSettings &settings) {
  if (running)
    return;

  currentSettings = settings;
  running = true;
  workerThread = std::thread(&AutoClicker::ClickThread, this);
}

void AutoClicker::Stop() {
  if (!running)
    return;

  running = false;
  if (workerThread.joinable()) {
    workerThread.join();
  }
}

bool AutoClicker::IsRunning() const { return running; }

void AutoClicker::Toggle(const ClickSettings &settings) {
  if (running) {
    Stop();
  } else {
    Start(settings);
  }
}

unsigned long AutoClicker::GetClickCount() const { return clickCount; }

void AutoClicker::ClickThread() {
  INPUT inputs[2] = {};

  // Set up common input structure
  inputs[0].type = INPUT_MOUSE;
  inputs[1].type = INPUT_MOUSE;

  while (running) {
    if (currentSettings.fixedPosition) {
      SetCursorPos(currentSettings.x, currentSettings.y);
    }

    DWORD downFlag = currentSettings.isLeftClick ? MOUSEEVENTF_LEFTDOWN
                                                 : MOUSEEVENTF_RIGHTDOWN;
    DWORD upFlag =
        currentSettings.isLeftClick ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP;

    inputs[0].mi.dwFlags = downFlag;
    inputs[1].mi.dwFlags = upFlag;

    SendInput(2, inputs, sizeof(INPUT));
    clickCount++;

    // Simple sleep for interval. For higher precision < 15ms, a different
    // approach would be needed. But for a basic auto clicker, Sleep is
    // sufficient and CPU friendly.
    Sleep(currentSettings.intervalMs);
  }
}
