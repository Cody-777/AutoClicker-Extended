#ifndef AUTOCLICKER_H
#define AUTOCLICKER_H

#include <atomic>
#include <string>
#include <thread>
#include <windows.h>

struct ClickSettings {
  int intervalMs = 100;
  bool isLeftClick = true;
  bool fixedPosition = false;
  int x = 0;
  int y = 0;
  int themeIndex = 0;
};

class AutoClicker {
public:
  AutoClicker();
  ~AutoClicker();

  void Start(const ClickSettings &settings);
  void Stop();
  bool IsRunning() const;
  void Toggle(const ClickSettings &settings);

  unsigned long GetClickCount() const;

private:
  void ClickThread();

  std::atomic<bool> running;
  std::atomic<unsigned long> clickCount;
  std::thread workerThread;
  ClickSettings currentSettings;
};

#endif // AUTOCLICKER_H
