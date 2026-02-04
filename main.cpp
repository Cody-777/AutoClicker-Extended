#include "AutoClicker.h"
#include "resource.h"
#include <commctrl.h>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>
#include <windows.h>

#pragma comment(lib, "comctl32.lib")

AutoClicker g_clicker;
HWND g_hDlg = NULL;

// Theme globals
HBRUSH g_hbrTheme = NULL;
COLORREF g_textColor = RGB(0, 0, 0);
COLORREF g_bkColor = GetSysColor(COLOR_3DFACE);

// Animation Structs
struct Star {
  float x, y;
  float speed;
  int size;
  COLORREF color;
};
std::vector<Star> g_stars;

struct MatrixStream {
  float x, y;
  float speed;
  int length;
};
std::vector<MatrixStream> g_matrix;

// Layout Replication
struct LayoutElement {
  RECT r;
  std::wstring text;
  bool isGroup;
};
std::vector<LayoutElement> g_layout;

// Forward declarations
void InitTheme(int themeIndex);
void UpdateAnimation(int themeIndex, int width, int height);

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

void InitTheme(int themeIndex) {
  if (themeIndex % 6 == 5) { // Space
    if (g_stars.empty()) {
      for (int i = 0; i < 100; i++) {
        Star s;
        s.x = (float)(rand() % 800);
        s.y = (float)(rand() % 600);
        s.speed = 1.0f + (rand() % 50) / 10.0f;
        s.size = (rand() % 2) + 1;
        int b = 150 + rand() % 105;
        s.color = RGB(b, b, b);
        g_stars.push_back(s);
      }
    }
  } else if (themeIndex % 6 == 2) { // Matrix
    if (g_matrix.empty()) {
      for (int i = 0; i < 40; i++) {
        MatrixStream m;
        m.x = (float)(rand() % 800);
        m.y = (float)(rand() % 600);
        m.speed = 2.0f + (rand() % 50) / 10.0f;
        m.length = 5 + (rand() % 15);
        g_matrix.push_back(m);
      }
    }
  }
}

void UpdateAnimation(int themeIndex, int width, int height) {
  if (themeIndex % 6 == 5) { // Space
    for (auto &s : g_stars) {
      s.y += s.speed;
      if (s.y > height) {
        s.y = 0;
        s.x = (float)(rand() % width);
      }
    }
  } else if (themeIndex % 6 == 2) { // Matrix
    for (auto &m : g_matrix) {
      m.y += m.speed;
      if (m.y - (m.length * 14) > height) {
        m.y = 0;
        m.x = (float)(rand() % width);
        m.length = 5 + (rand() % 15);
      }
    }
  }
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
    g_hbrTheme = CreateSolidBrush(g_bkColor);
    break;
  }

  // Force full repaint of children too (because of WS_CLIPCHILDREN)
  RedrawWindow(g_hDlg, NULL, NULL,
               RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

void DrawThemeBackground(HDC hdc, RECT r, int themeIndex) {
  // Fill background first
  // Fill background
  // Use g_hbrTheme so that Hatch brushes (Blueprint) show the pattern.
  // Ensure BkColor/Mode are set for the gaps in the hatch.
  COLORREF oldBk = SetBkColor(hdc, g_bkColor);
  int oldMode = SetBkMode(hdc, OPAQUE);

  FillRect(hdc, &r, g_hbrTheme);

  SetBkColor(hdc, oldBk);
  SetBkMode(hdc, oldMode);

  if (themeIndex % 6 == 5) { // Space
    for (const auto &s : g_stars) {
      SetPixel(hdc, (int)s.x, (int)s.y, s.color);
      if (s.size > 1) {
        SetPixel(hdc, (int)s.x + 1, (int)s.y, s.color);
        SetPixel(hdc, (int)s.x, (int)s.y + 1, s.color);
        SetPixel(hdc, (int)s.x + 1, (int)s.y + 1, s.color);
      }
    }
  } else if (themeIndex % 6 == 2) { // Matrix
    SetBkMode(hdc, TRANSPARENT);
    HFONT hFont =
        CreateFont(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                   DEFAULT_PITCH | FF_MODERN, L"Consolas");
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);

    for (const auto &m : g_matrix) {
      for (int i = 0; i < m.length; i++) {
        // Head is bright, tail fades (LUT or simple logic)
        int green = 255;
        if (i > 0)
          green = 150 - (i * 10);
        if (green < 50)
          green = 50;

        SetTextColor(hdc, RGB(0, green, 0));

        char c = (rand() % 2) ? '1' : '0';
        // Draw at y - index * spacing
        TextOutA(hdc, (int)m.x, (int)m.y - (i * 14), &c, 1);
      }
    }
    SelectObject(hdc, hOld);
    DeleteObject(hFont);
  }
}

// Callback to scan controls
BOOL CALLBACK ScanEnumProc(HWND hwnd, LPARAM lParam) {
  // Skip if it's the dialog itself? No, EnumChild only does children.

  // Check identifiers
  int id = GetDlgCtrlID(hwnd);

  // Get Class Name
  wchar_t className[256];
  GetClassNameW(hwnd, className, 256);

  // Get Style
  LONG style = GetWindowLong(hwnd, GWL_STYLE);

  // Identify what we want to replicate
  // Groups: Button class + BS_GROUPBOX
  // Labels: Static class + (id == -1 usually or specific IDs)

  bool isGroup = (wcscmp(className, L"Button") == 0 &&
                  (style & BS_GROUPBOX) == BS_GROUPBOX);
  // Only capture layout statics (ID -1), leave dynamic statics (Status/Clicks)
  // alone
  bool isStatic = (wcscmp(className, L"Static") == 0 && id == -1);

  if (isGroup || isStatic) {
    // We only care about Labels that have text and aren't simple icons (like
    // SS_ICON) For this app, simply capturing all visible Statics is fine.

    LayoutElement el;
    el.isGroup = isGroup;

    // Get Rect relative to Client
    RECT rWindow;
    GetWindowRect(hwnd, &rWindow);
    POINT p1 = {rWindow.left, rWindow.top};
    POINT p2 = {rWindow.right, rWindow.bottom};
    ScreenToClient(g_hDlg, &p1);
    ScreenToClient(g_hDlg, &p2);
    el.r = {p1.x, p1.y, p2.x, p2.y};

    // Get Text
    int len = GetWindowTextLengthW(hwnd);
    if (len > 0) {
      wchar_t *buf = new wchar_t[len + 1];
      GetWindowTextW(hwnd, buf, len + 1);
      el.text = buf;
      delete[] buf;
    }

    g_layout.push_back(el);

    // Hide original
    ShowWindow(hwnd, SW_HIDE);
  }

  return TRUE;
}

void ScanLayout() {
  if (!g_layout.empty())
    return; // Only scan once
  EnumChildWindows(g_hDlg, ScanEnumProc, 0);
}

// Custom Draw Helper
void DrawUIOverlay(HDC hdc) {
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, g_textColor);

  HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
  HFONT hOld = (HFONT)SelectObject(hdc, hFont);

  for (const auto &el : g_layout) {
    if (el.isGroup) {
      // Draw Title (Inset slightly)
      if (!el.text.empty()) {
        RECT rText = el.r;
        rText.left += 10;
        rText.top -= 7;

        // Measure text exact size
        SIZE size;
        GetTextExtentPoint32W(hdc, el.text.c_str(), (int)el.text.length(),
                              &size);
        rText.right = rText.left + size.cx;
        rText.bottom = rText.top + size.cy;

        // Clip out the text area so FrameRect doesn't draw through it
        SaveDC(hdc);
        ExcludeClipRect(hdc, rText.left, rText.top, rText.right, rText.bottom);

        // Draw Box
        HBRUSH borderBrush = CreateSolidBrush(g_textColor);
        FrameRect(hdc, &el.r, borderBrush);
        DeleteObject(borderBrush);

        RestoreDC(hdc, -1); // Restore clip region

        // NOW Draw Text
        TextOutW(hdc, rText.left, rText.top, el.text.c_str(),
                 (int)el.text.length());
      } else {
        // No text, just box
        HBRUSH borderBrush = CreateSolidBrush(g_textColor);
        FrameRect(hdc, &el.r, borderBrush);
        DeleteObject(borderBrush);
      }
    } else {
      // Draw Label
      // Check if it has text (we filtered empty ones usually)
      if (!el.text.empty()) {
        // Vertically center? Statics usually align top-left.
        TextOutW(hdc, el.r.left, el.r.top, el.text.c_str(),
                 (int)el.text.length());
      }
    }
  }

  SelectObject(hdc, hOld);
}

INT_PTR CALLBACK DialogProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  switch (uMsg) {
  case WM_INITDIALOG: {
    g_hDlg = hDlg;
    // Load settings and apply to UI
    ClickSettings s = LoadSettings();
    SetUIFromSettings(s);

    // Init Theme and Layout
    srand((unsigned int)time(NULL));
    InitTheme(s.themeIndex);
    UpdateTheme(s.themeIndex);

    // Scan and hide default controls for replication
    ScanLayout();

    // Clear text from checkboxes... NO, we want them visible and OPAQUE now.
    // In Hybrid mode, interactive controls draw themselves.
    // So we do NOT clear their text. They will be solid blocks.

    // Init Hotkey Control
    SendDlgItemMessage(hDlg, IDC_HOTKEY_FIELD, HKM_SETHOTKEY,
                       MAKEWORD(s.hotkeyVk, s.hotkeyMod), 0);

    // Register Hotkey
    RegisterHotKey(hDlg, HK_START_STOP, GetWinModFromCommCtrl(s.hotkeyMod),
                   s.hotkeyVk);

    // Timer for updating click count and animation
    SetTimer(hDlg, 1, 33, NULL);
    UpdateUIState();
    return TRUE;
  }

  case WM_ERASEBKGND:
    return 1; // Prevent flicker, we draw in Paint

  case WM_CTLCOLOREDIT:
  case WM_CTLCOLORLISTBOX:
  case WM_CTLCOLORBTN:
  case WM_CTLCOLORSTATIC: {
    HDC hdcStatic = (HDC)wParam;
    SetTextColor(hdcStatic, g_textColor);
    SetBkColor(hdcStatic, g_bkColor);
    SetBkMode(hdcStatic, OPAQUE); // Make inputs solid
    return (INT_PTR)g_hbrTheme;
  }

  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hDlg, &ps);
    RECT r;
    GetClientRect(hDlg, &r);

    // Manual Double Buffering (Restored)
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP hBM = CreateCompatibleBitmap(hdc, r.right, r.bottom);
    HBITMAP hOld = (HBITMAP)SelectObject(memDC, hBM);

    ClickSettings s = LoadSettings();
    DrawThemeBackground(memDC, r, s.themeIndex);
    DrawUIOverlay(memDC);

    BitBlt(hdc, 0, 0, r.right, r.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, hOld);
    DeleteObject(hBM);
    DeleteDC(memDC);

    EndPaint(hDlg, &ps);
    return 0;
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
      // Let's use a static variable or just rely on LoadSettings since we
      // save on exit? No, we haven't saved yet. Let's adding themeIndex to
      // GetSettingsFromUI is hard because it's not a control. Hack: Load,
      // increment, Save, Update.
      ClickSettings current = LoadSettings();
      current.themeIndex++;
      InitTheme(current.themeIndex); // Ensure initialized if switching to it
      SaveSettings(current);         // Save immediately
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
      // HOTKEYF_ALT (0x04) RegisterHotKey expects MOD_SHIFT (0x04),
      // MOD_CONTROL (0x02), MOD_SHIFT (0x04) Wait, Windows API is confusing
      // here. CommCtrl.h: HOTKEYF_SHIFT   0x01 HOTKEYF_CONTROL 0x02
      // HOTKEYF_ALT 0x04
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
      // and convert when registering. ACTUALLY, checking AutoClicker.h, I
      // just added `hotkeyVk` and `hotkeyMod`. Let's decide `hotkeyMod`
      // stores the COMMCTRL flags (for easy set back to control).

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
    if (wParam == 1) {
      UpdateClickCount();

      // Animation update
      ClickSettings s = LoadSettings();
      if (s.themeIndex % 6 == 5 || s.themeIndex % 6 == 2) {
        RECT r;
        GetClientRect(hDlg, &r);
        UpdateAnimation(s.themeIndex, r.right, r.bottom);
        InvalidateRect(hDlg, NULL,
                       FALSE); // FALSE to not erase background (flicker
                               // redudancy, since we handle Erase)
        // Actually with WS_EX_COMPOSITED, InvalidateRect(..., FALSE) +
        // WM_ERASEBKGND handling is good.
      }
    }
    break;

  case WM_CTLCOLORDLG:
    return (INT_PTR)GetStockObject(
        NULL_BRUSH); // Return hollow brush for dialog background

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
