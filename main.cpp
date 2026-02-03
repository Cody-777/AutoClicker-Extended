#include "AutoClicker.h"
#include "resource.h"
#include <commctrl.h>
#include <fstream>
#include <string>
#include <windows.h>

#pragma comment(lib, "comctl32.lib")

AutoClicker g_clicker;
HWND g_hDlg = NULL;

// Theme globals
HBRUSH g_hbrTheme = NULL;
COLORREF g_textColor = RGB(0, 0, 0);
COLORREF g_bkColor = GetSysColor(COLOR_3DFACE);

// Helper to format hotkey string
std::wstring GetHotkeyString(int vk, int mod) {
  if (vk == 0)
    return L"None";

  std::wstring s = L"";
  if (mod & HOTKEYF_CONTROL)
    s += L"Ctrl + ";
  if (mod & HOTKEYF_SHIFT)
    s += L"Shift + ";
  if (mod & HOTKEYF_ALT)
    s += L"Alt + ";

  UINT scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
  wchar_t buffer[32];
  if (GetKeyNameTextW(scanCode << 16, buffer, 32)) {
    s += buffer;
  } else {
    s += L"Key";
  }
  return s;
}

UINT GetWinModFromCommCtrl(int mod) {
  UINT fsModifiers = 0;
  if (mod & HOTKEYF_SHIFT)
    fsModifiers |= MOD_SHIFT;
  if (mod & HOTKEYF_CONTROL)
    fsModifiers |= MOD_CONTROL;
  if (mod & HOTKEYF_ALT)
    fsModifiers |= MOD_ALT;
  return fsModifiers;
}

const int HK_START_STOP = 1;
const char *SETTINGS_FILE = "settings.dat";

void SaveSettings(const ClickSettings &settings) {
  std::ofstream file(SETTINGS_FILE, std::ios::binary);
  if (file.is_open()) {
    file.write(reinterpret_cast<const char *>(&settings),
               sizeof(ClickSettings));
  }
}

ClickSettings LoadSettings() {
  ClickSettings s;
  // Initialize with defaults in case file doesn't exist or is corrupt
  s.intervalMs = 100;
  s.isLeftClick = true;
  s.fixedPosition = false;
  s.x = 0;
  s.y = 0;
  s.themeIndex = 0;

  std::ifstream file(SETTINGS_FILE, std::ios::binary);
  if (file.is_open()) {
    file.read(reinterpret_cast<char *>(&s), sizeof(ClickSettings));
  }
  return s;
}

void UpdateUIState() {
  bool running = g_clicker.IsRunning();
  SetDlgItemText(g_hDlg, IDC_BTN_STARTSTOP, running ? L"Stop" : L"Start");

  // Update button text with hotkey
  ClickSettings s = LoadSettings();
  std::wstring btnText = running ? L"Stop (" : L"Start (";
  btnText += GetHotkeyString(s.hotkeyVk, s.hotkeyMod);
  btnText += L")";
  SetDlgItemText(g_hDlg, IDC_BTN_STARTSTOP, btnText.c_str());

  SetDlgItemText(g_hDlg, IDC_STAT_STATUS,
                 running ? L"Status: Running" : L"Status: Stopped");

  // Disable inputs while running
  EnableWindow(GetDlgItem(g_hDlg, IDC_EDIT_INTERVAL), !running);
  EnableWindow(GetDlgItem(g_hDlg, IDC_RADIO_LEFT), !running);
  EnableWindow(GetDlgItem(g_hDlg, IDC_RADIO_RIGHT), !running);
  EnableWindow(GetDlgItem(g_hDlg, IDC_RADIO_CURRENT), !running);
  EnableWindow(GetDlgItem(g_hDlg, IDC_RADIO_FIXED), !running);

  // only enable coords if Fixed is checked AND not running
  bool fixed = IsDlgButtonChecked(g_hDlg, IDC_RADIO_FIXED) == BST_CHECKED;
  EnableWindow(GetDlgItem(g_hDlg, IDC_EDIT_X), !running && fixed);
  EnableWindow(GetDlgItem(g_hDlg, IDC_EDIT_Y), !running && fixed);
}

void UpdateClickCount() {
  if (g_hDlg) {
    std::wstring s = L"Clicks: " + std::to_wstring(g_clicker.GetClickCount());
    SetDlgItemText(g_hDlg, IDC_STAT_CLICKS, s.c_str());
  }
}

ClickSettings GetSettingsFromUI() {
  ClickSettings s;
  s.intervalMs = GetDlgItemInt(g_hDlg, IDC_EDIT_INTERVAL, NULL, FALSE);
  if (s.intervalMs < 1)
    s.intervalMs = 1; // Minimum safety

  s.isLeftClick = (IsDlgButtonChecked(g_hDlg, IDC_RADIO_LEFT) == BST_CHECKED);
  s.fixedPosition =
      (IsDlgButtonChecked(g_hDlg, IDC_RADIO_FIXED) == BST_CHECKED);
  s.x = GetDlgItemInt(g_hDlg, IDC_EDIT_X, NULL, TRUE);
  s.y = GetDlgItemInt(g_hDlg, IDC_EDIT_Y, NULL, TRUE);
  return s;
}

void SetUIFromSettings(const ClickSettings &s) {
  SetDlgItemInt(g_hDlg, IDC_EDIT_INTERVAL, s.intervalMs, FALSE);
  CheckRadioButton(g_hDlg, IDC_RADIO_LEFT, IDC_RADIO_RIGHT,
                   s.isLeftClick ? IDC_RADIO_LEFT : IDC_RADIO_RIGHT);
  CheckRadioButton(g_hDlg, IDC_RADIO_CURRENT, IDC_RADIO_FIXED,
                   s.fixedPosition ? IDC_RADIO_FIXED : IDC_RADIO_CURRENT);
  SetDlgItemInt(g_hDlg, IDC_EDIT_X, s.x, TRUE);
  SetDlgItemInt(g_hDlg, IDC_EDIT_Y, s.y, TRUE);
}

HBRUSH CreateSpaceBrush() {
  const int size = 64;
  HDC hdc = GetDC(NULL);
  HDC memDC = CreateCompatibleDC(hdc);
  HBITMAP hBitmap = CreateCompatibleBitmap(hdc, size, size);
  HBITMAP hOldBitmap = (HBITMAP)SelectObject(memDC, hBitmap);

  // Fill black
  RECT r = {0, 0, size, size};
  FillRect(memDC, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));

  // Draw stars (random white pixels)
  for (int i = 0; i < 15; i++) {
    int x = rand() % size;
    int y = rand() % size;
    SetPixel(memDC, x, y, RGB(255, 255, 255));

    // Fainter stars
    x = rand() % size;
    y = rand() % size;
    SetPixel(memDC, x, y, RGB(150, 150, 150));
  }

  SelectObject(memDC, hOldBitmap);
  HBRUSH hBrush = CreatePatternBrush(hBitmap);

  DeleteObject(hBitmap);
  DeleteDC(memDC);
  ReleaseDC(NULL, hdc);

  return hBrush;
}

void UpdateTheme(int themeIndex) {
  if (g_hbrTheme)
    DeleteObject(g_hbrTheme);

  switch (themeIndex % 6) {
  case 0: // Default
    g_bkColor = GetSysColor(COLOR_3DFACE);
    g_textColor = GetSysColor(COLOR_WINDOWTEXT);
    g_hbrTheme = GetSysColorBrush(COLOR_3DFACE);
    break;
  case 1: // Dark Mode
    g_bkColor = RGB(50, 50, 50);
    g_textColor = RGB(255, 255, 255);
    g_hbrTheme = CreateSolidBrush(g_bkColor);
    break;
  case 2: // Matrix
    g_bkColor = RGB(0, 0, 0);
    g_textColor = RGB(0, 255, 0);
    g_hbrTheme = CreateSolidBrush(g_bkColor);
    break;
  case 3: // Blueprint
    g_bkColor = RGB(20, 40, 100);
    g_textColor = RGB(200, 220, 255);
    g_hbrTheme = CreateHatchBrush(HS_CROSS, RGB(30, 60, 150)); // Grid pattern
    break;
  case 4: // Sunset
    g_bkColor = RGB(255, 100, 50);
    g_textColor = RGB(50, 0, 0);
    g_hbrTheme = CreateSolidBrush(g_bkColor);
    break;
  case 5: // Space
    g_bkColor = RGB(0, 0, 20);
    g_textColor = RGB(200, 200, 255);
    g_hbrTheme = CreateSpaceBrush();
    break;
  }

  InvalidateRect(g_hDlg, NULL, TRUE);
}

INT_PTR CALLBACK DialogProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  switch (uMsg) {
  case WM_INITDIALOG: {
    g_hDlg = hDlg;
    // Load settings and apply to UI
    ClickSettings s = LoadSettings();
    SetUIFromSettings(s);
    UpdateTheme(s.themeIndex);

    // Init Hotkey Control
    SendDlgItemMessage(hDlg, IDC_HOTKEY_FIELD, HKM_SETHOTKEY,
                       MAKEWORD(s.hotkeyVk, s.hotkeyMod), 0);

    // Register Hotkey
    RegisterHotKey(hDlg, HK_START_STOP, GetWinModFromCommCtrl(s.hotkeyMod),
                   s.hotkeyVk);

    // Timer for updating click count
    SetTimer(hDlg, 1, 100, NULL);
    UpdateUIState();
    return TRUE;
  }

  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDC_BTN_STARTSTOP:
      g_clicker.Toggle(GetSettingsFromUI());
      UpdateUIState();
      break;

    case IDC_BTN_THEME: {
      ClickSettings s = GetSettingsFromUI();
      // We need to know current theme index.
      // Since GetSettingsFromUI doesn't read it from UI (it's hidden state),
      // we must load it or store it.
      // Let's use a static variable or just rely on LoadSettings since we save
      // on exit? No, we haven't saved yet. Let's adding themeIndex to
      // GetSettingsFromUI is hard because it's not a control. Hack: Load,
      // increment, Save, Update.
      ClickSettings current = LoadSettings();
      current.themeIndex++;
      SaveSettings(current); // Save immediately
      UpdateTheme(current.themeIndex);
    } break;

    case IDC_RADIO_CURRENT:
    case IDC_RADIO_FIXED:
      UpdateUIState();
      break;

    case IDC_BTN_SETHOTKEY: {
      // Get key from control
      LRESULT hk =
          SendDlgItemMessage(hDlg, IDC_HOTKEY_FIELD, HKM_GETHOTKEY, 0, 0);
      int vk = LOBYTE(LOWORD(hk));
      int mod = HIBYTE(LOWORD(hk)); // This returns HOTKEYF_* flags

      // Validating mod flags for RegisterHotKey
      // HKM_GETHOTKEY returns HOTKEYF_SHIFT (0x01), HOTKEYF_CONTROL (0x02),
      // HOTKEYF_ALT (0x04) RegisterHotKey expects MOD_SHIFT (0x04), MOD_CONTROL
      // (0x02), MOD_ALT (0x01) Wait, Windows API is confusing here. CommCtrl.h:
      // HOTKEYF_SHIFT   0x01
      // HOTKEYF_CONTROL 0x02
      // HOTKEYF_ALT     0x04
      //
      // WinUser.h:
      // MOD_ALT      0x0001
      // MOD_CONTROL  0x0002
      // MOD_SHIFT    0x0004
      //
      // So they are SWAPPED. We need to convert.

      UINT fsModifiers = GetWinModFromCommCtrl(mod);

      // Update Settings
      ClickSettings s = LoadSettings(); // Load to preserve other settings

      // Store the COMMCTRL modifiers for display/control, but we might need
      // logic for RegisterHotKey Let's store what we get from HKM_GETHOTKEY,
      // and convert when registering. ACTUALLY, checking AutoClicker.h, I just
      // added `hotkeyVk` and `hotkeyMod`. Let's decide `hotkeyMod` stores the
      // COMMCTRL flags (for easy set back to control).

      s.hotkeyVk = vk;
      s.hotkeyMod = mod;
      SaveSettings(s);

      // Re-register
      UnregisterHotKey(hDlg, HK_START_STOP);
      if (!RegisterHotKey(hDlg, HK_START_STOP, fsModifiers, vk)) {
        MessageBox(hDlg, L"Failed to register hotkey!", L"Error",
                   MB_OK | MB_ICONERROR);
      }

      UpdateUIState();

      // Update label
      std::wstring label =
          L"Hotkeys: " + GetHotkeyString(vk, mod) + L" to Start/Stop";
      SetDlgItemText(hDlg, IDC_STAT_HOTKEY, label.c_str());
    } break;

    case IDCANCEL:
      EndDialog(hDlg, 0);
      return TRUE;
    }
    break;

  case WM_HOTKEY:
    if (wParam == HK_START_STOP) {
      g_clicker.Toggle(GetSettingsFromUI());
      UpdateUIState();
    }
    break;

  case WM_TIMER:
    UpdateClickCount();
    break;

  case WM_CTLCOLORDLG:
  case WM_CTLCOLORSTATIC:
  case WM_CTLCOLORBTN: {
    HDC hdc = (HDC)wParam;
    SetBkColor(hdc, g_bkColor);
    SetTextColor(hdc, g_textColor);
    if (uMsg == WM_CTLCOLORDLG)
      return (INT_PTR)g_hbrTheme;
    SetBkMode(hdc, TRANSPARENT);
    return (INT_PTR)g_hbrTheme;
  }

  case WM_CLOSE:
    EndDialog(hDlg, 0);
    return TRUE;

  case WM_DESTROY:
    UnregisterHotKey(hDlg, HK_START_STOP);
    g_clicker.Stop();
    {
      ClickSettings s = GetSettingsFromUI();
      // Preserve theme index
      ClickSettings saved = LoadSettings();
      s.themeIndex = saved.themeIndex;
      SaveSettings(s);
    }
    break;
  }
  return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nShowCmd) {
  InitCommonControls();
  return DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAINDIALOG), NULL,
                   DialogProc);
}
