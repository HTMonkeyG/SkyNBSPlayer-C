/**
 * Sky: CotL NBS Music Player
 * 
 * An autoplay tool designed for Netease edition of Sky on Windows.
 * 
 * Copyright (c) 2025 HTMonkeyG
 * 
 * (https://www.github.com/HTMonkeyG/SkyNBSPlayer-C)
 */

#include "main.h"

HWND hSkyGameWnd;
HANDLE hMutex, hHotkeyThread;
DWORD threadId;
NBS nbs;
Vector_t builtTicks = {0};
f32 tempo;
SkyMusicTick_t *currentTick = NULL;
SkyNBSPlayerOptions_t options = {
  .printVersion = 0,
  .printHelp = 0,
  .runOnce = 0,
  .hkOpen = {
    .mod = MOD_CONTROL,
    .vk = 'O'
  },
  .hkPlay = {
    .mod = MOD_ALT,
    .vk = 'O'
  },
  .hkPause = {
    .mod = MOD_ALT,
    .vk = 'O'
  },
  .hkStop = {
    .mod = MOD_ALT,
    .vk = 'I'
  },
  .playerOptions = {
    .highTps = 0,
    .randomShift = 0,
    .shiftStrength = 0,
    .shiftType = 0,
    .fps = 60
  }
};
SkyMusicPlayer_t player = {0};
wchar_t nbsPath[MAX_PATH] = {0}
  , exePath[MAX_PATH];

i32 pickFile(const wchar_t *path, wchar_t *result, i32 maxLength) {
  OPENFILENAMEW ofn = {0};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = NULL;
  ofn.lpstrFile = result;
  ofn.nMaxFile = maxLength;
  ofn.lpstrFilter = L"All(*.*)\0*.*\0Note Block Studio(*.nbs)\0*.nbs\0Sky Studio(*.txt)\0*.txt\0\0";
  ofn.nFilterIndex = 2;
  ofn.lpstrFileTitle = NULL;
  ofn.lpstrInitialDir = path;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;
  return GetOpenFileNameW(&ofn);
}

i32 readAndBuildNBS(
  wchar_t *path,
  SkyNBSPlayerOptions_t *options,
  Vector_t *builtTicks
) {
  FILE *file;
  NBS nbs;
  size_t fileSize;
  i32 err = 0
    , result;

  if (!path || !builtTicks || !options)
    return 0;

  LOG(L"Reading NBS file: %s\n", nbsPath);

  file = _wfopen(path, L"rb");
  if (!file)
    return 0;

  fseek(file, 0, SEEK_END);
  fileSize = ftell(file);
  rewind(file);

  i08 *buffer = malloc(fileSize);
  fread(buffer, 1, fileSize, file);
  result = readNBSFile(buffer, fileSize, &nbs, &err);
  free(buffer);
  fclose(file);

  LOG(L"NBS reader stopped with state: %d\n", err);

  if (!result) {
    MBError(L"文件读取失败", 0);
    return 0;
  }
  if (nbs.header.tempo < 0) {
    MBError(L"无效NBS文件", 0);
    return 0;
  }
  LOG(L"Tempo: %f\n", (f32)nbs.header.tempo / 100.);
  LOG(L"Compiling notes...\n");
  buildTicksFrom(&options->playerOptions, &nbs, builtTicks);
  freeNBSFile(&nbs);

  return 1;
}

void cfgCallback(const wchar_t *key, const wchar_t *value) {
  wchar_t *p;
  LOG(L"- %s: %s\n", key, value);

  if (!wcscmp(key, L"high_tps"))
    options.playerOptions.highTps = (wcstof(value, &p) != 0);
  else if (!wcscmp(key, L"frame_rate"))
    options.playerOptions.fps = wcstoul(value, &p, 10);
}

void argCallback(const wchar_t *value, int count, int *state) {
  LOG(L"- %d %s\n", count, value);

  if (!count && (!wcscmp(value, L"i") || !wcscmp(value, L"input")))
    // -i <file to read>
    *state = 1;
  
  if (*state == 1 && count > 0) {
    // -i <file to read>
    LOG(L"Input file: %s\n", value);
    wcscpy_s(nbsPath, MAX_PATH, value);
    *state = 0;
  }
  if (*state == AS_INITIAL && count == 2) {
    // skycol-nbs.exe <file to read>
    LOG(L"Input file: %s\n", value);
    wcscpy_s(nbsPath, MAX_PATH, value);
    *state = 0;
  }
}

DWORD WINAPI hotkeyThread(LPVOID lpParam) {
  MSG msg;
 
  if (!RegisterHotKey(NULL, 1, options.hkOpen.mod, options.hkOpen.vk)) {
    MBError(L"注册打开文件快捷键失败", 0);
    return 1;
  }
  if (!RegisterHotKey(NULL, 2, options.hkPlay.mod, options.hkPlay.vk)) {
    MBError(L"注册播放快捷键失败", 0);
    return 1;
  }

  while (GetMessageW(&msg, NULL, 0, 0)) {
    if (msg.message == WM_QUIT)
      break;
    if (msg.message != WM_HOTKEY || GetForegroundWindow() != hSkyGameWnd)
      continue;
    if (msg.wParam == 1) {
      // Open file.
      snMusicStop(&player);
      // Try to browse file.
      if (!pickFile(exePath, nbsPath, MAX_PATH)) {
        // Browse failed.
        if (CommDlgExtendedError())
          MBError(L"选择文件失败", 0);
        else
          LOG(L"User cancelled file selection.");
        continue;
      }
      // Browse successed.
      vec_init(&builtTicks, sizeof(SkyMusicTick_t));
      snRemovePlayer(&player);
      if (!readAndBuildNBS(nbsPath, &options, &builtTicks))
        continue;
      snCreatePlayer(&player, &options.playerOptions, hSkyGameWnd, &builtTicks);
      SetForegroundWindow(hSkyGameWnd);
    } else if (msg.wParam == 2) {
      // Play current file.
      if (player.state > 0) {
        snMusicPlay(&player);
        LOG(L"Play, %d.\n", player.state);
      } else if (player.state == STOPPED_PROG || player.state == STOPPED_ESC) {
        snMusicResume(&player);
        LOG(L"Resume, %d.\n", player.state);
      } else if (player.state == PLAYING) {
        snMusicPause(&player);
        LOG(L"Pause, %d.\n", player.state);
      }
    }
  }

  UnregisterHotKey(NULL, 1);
  UnregisterHotKey(NULL, 2);
  return 0;
}

/** Initialize the software. */
i32 initPlayer() {
  wchar_t cfgPath[MAX_PATH]
    , *p;

#ifndef DEBUG_CONSOLE
  // Close console window.
  HWND hWnd = GetConsoleWindow();
  if (hWnd != NULL)
    ShowWindow(hWnd, SW_HIDE);
#endif

#ifndef DEBUG_NO_INSTANCE_DUPLICATE_CHECK
  // Check whether another instance is running.
  hMutex = CreateMutexW(NULL, TRUE, L"__SKY_NBS__");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    MBError(L"已有实例正在运行", 0);
    return 0;
  }
#endif

  // Get config file path.
  if (!GetModuleFileNameW(NULL, exePath, MAX_PATH)) {
    LOG(L"Failed to get module path.");
    goto ErrReadCfg;
  }
  p = wcsrchr(exePath, L'\\');
  if (!p) {
    LOG(L"Failed to initialize config file path.", 0);
    goto ErrReadCfg;
  }
  *p = 0;
  wcscpy(cfgPath, exePath);
  wcscat(cfgPath, L"\\skynbs-config.txt");

  // Read config file.
  FILE *f = _wfopen(cfgPath, L"r");
  if (!f) {
    LOG(L"Missing config file.");

ErrReadCfg:
    MBError(L"读取配置文件失败", 0);
    return 0;
  }

  // Read argv and config file.
  LOG(L"Reading config: %s...\n", cfgPath);
  buildConfigFrom(f, cfgCallback);
  fclose(f);
  LOG(L"Reading argv...\n");
  buildArgFrom(argCallback);

  // Get game window handle.
  hSkyGameWnd = FindWindowW(NULL, L"光·遇");
#ifndef DEBUG_NO_GAME_RUNNING_CHECK
  while (!hSkyGameWnd) {
    if (MBError(L"游戏未运行，是否重试？", MB_YESNO) == IDNO)
      return 0;
    hSkyGameWnd = FindWindowW(NULL, L"光·遇");
  }
#endif
  LOG(L"Get game window handle: 0x%X.\n", hSkyGameWnd);
  SetForegroundWindow(hSkyGameWnd);

  // Create hotkey listener.
  hHotkeyThread = CreateThread(NULL, 0, hotkeyThread, 0, 0, &threadId);
  if (!hHotkeyThread) {
    MBError(L"创建子线程失败", 0);
    return 0;
  }

  return 1;
}

int main() {
  setlocale(LC_ALL, "zh_CN.UTF-8");

  // Initialize.
  if (!initPlayer())
    return 1;

  if (wcslen(nbsPath))
    // If specified nbs file through command line, then only play this file.
    options.runOnce = 1;

  switch (player.state) {
    case STOPPED_EOF:
      LOG(L"Completed.");
      break;
    case STOPPED_ESC:
      LOG(L"Stopped by exit piano keyboard.");
      break;
    case STOPPED_PROG:
      LOG(L"Stopped by program.");
      break;
    case PAUSED_BG:
      LOG(L"Paused by leave game window.");
      break;
    default:
      break;
  }
  while (1);
  ReleaseMutex(hMutex);
  CloseHandle(hMutex);
  return 0;
}
