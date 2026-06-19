#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <mmsystem.h>
#include "resource.h"
#pragma comment(lib, "comctl32.lib")

#define IDC_LISTVIEW 1001
#define IDC_TIMEFROM 1003
#define IDC_TIMETO 1004
#define IDC_VOLUME 1005
#define IDC_ADD 1006
#define IDC_SAVE 1007
#define IDC_DELETE 1008
#define IDC_DEFVOL 1009
#define IDC_HIDE 1010
#define IDC_TIMELABEL 1011
#define IDC_AUTOSTART 1012
#define IDC_STARTMINIMIZED 1013
#define IDC_DAY_MON 1014
#define IDC_DAY_TUE 1015
#define IDC_DAY_WED 1016
#define IDC_DAY_THU 1017
#define IDC_DAY_FRI 1018
#define IDC_DAY_SAT 1019
#define IDC_DAY_SUN 1020
#define IDC_ALLDAY 1021
#define IDC_FORCERUN 1022
#define IDC_CURVOL 1023

#define WM_TRAYICON (WM_USER + 1)

HINSTANCE hInst;
HWND hWndMain, hListView, hTimeFrom, hTimeTo, hVolume, hDefVol, hTimeLabel, hCurVolLabel, hAllDay;
HWND hAutoStart, hStartMinimized;
HWND hDaysCB[7];
NOTIFYICONDATAW nid;
int iLastMin = -1;
int iLastSec = -1;
wchar_t szIniFile[MAX_PATH];
const wchar_t* daysStr[] = {L"Пн", L"Вт", L"Ср", L"Чт", L"Пт", L"Сб", L"Вс"};

void LogEvent(const wchar_t* message) {
    wchar_t logPath[MAX_PATH];
    GetModuleFileNameW(NULL, logPath, MAX_PATH);
    wchar_t* ptr = wcsrchr(logPath, L'\\');
    if (ptr) { *ptr = 0; wcscat(logPath, L"\\schedule.log"); }
    
    FILE* f = _wfopen(logPath, L"a, ccs=UTF-8");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fwprintf(f, L"[%02d.%02d.%04d %02d:%02d:%02d] %ls\n", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond, message);
        fclose(f);
    }
}

BOOL CALLBACK EnumChildProc(HWND child, LPARAM font) {
    SendMessageW(child, WM_SETFONT, font, TRUE);
    return TRUE;
}

int GetSysVolume() {
    int vol = 0;
    HMIXER hMixer;
    if (mixerOpen(&hMixer, 0, 0, 0, 0) == MMSYSERR_NOERROR) {
        MIXERLINEW ml = {0};
        ml.cbStruct = sizeof(MIXERLINEW);
        ml.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
        if (mixerGetLineInfoW((HMIXEROBJ)hMixer, &ml, MIXER_GETLINEINFOF_COMPONENTTYPE) == MMSYSERR_NOERROR) {
            MIXERLINECONTROLSW mlc = {0};
            MIXERCONTROLW mc = {0};
            mlc.cbStruct = sizeof(MIXERLINECONTROLSW);
            mlc.dwLineID = ml.dwLineID;
            mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
            mlc.cControls = 1;
            mlc.cbmxctrl = sizeof(MIXERCONTROLW);
            mlc.pamxctrl = &mc;
            if (mixerGetLineControlsW((HMIXEROBJ)hMixer, &mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE) == MMSYSERR_NOERROR) {
                MIXERCONTROLDETAILS mcd = {0};
                MIXERCONTROLDETAILS_UNSIGNED mcdu = {0};
                mcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
                mcd.dwControlID = mc.dwControlID;
                mcd.cChannels = 1;
                mcd.cMultipleItems = 0;
                mcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
                mcd.paDetails = &mcdu;
                if (mixerGetControlDetailsW((HMIXEROBJ)hMixer, &mcd, MIXER_GETCONTROLDETAILSF_VALUE) == MMSYSERR_NOERROR) {
                    vol = (int)((mcdu.dwValue * 100) / 65535);
                }
            }
        }
        mixerClose(hMixer);
    }
    return vol;
}

void SetSysVolume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    HMIXER hMixer;
    if (mixerOpen(&hMixer, 0, 0, 0, 0) == MMSYSERR_NOERROR) {
        MIXERLINEW ml = {0};
        ml.cbStruct = sizeof(MIXERLINEW);
        ml.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
        if (mixerGetLineInfoW((HMIXEROBJ)hMixer, &ml, MIXER_GETLINEINFOF_COMPONENTTYPE) == MMSYSERR_NOERROR) {
            MIXERLINECONTROLSW mlc = {0};
            MIXERCONTROLW mc = {0};
            mlc.cbStruct = sizeof(MIXERLINECONTROLSW);
            mlc.dwLineID = ml.dwLineID;
            mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
            mlc.cControls = 1;
            mlc.cbmxctrl = sizeof(MIXERCONTROLW);
            mlc.pamxctrl = &mc;
            if (mixerGetLineControlsW((HMIXEROBJ)hMixer, &mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE) == MMSYSERR_NOERROR) {
                MIXERCONTROLDETAILS mcd = {0};
                MIXERCONTROLDETAILS_UNSIGNED mcdu = {0};
                mcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
                mcd.dwControlID = mc.dwControlID;
                mcd.cChannels = 1;
                mcd.cMultipleItems = 0;
                mcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
                mcd.paDetails = &mcdu;
                mcdu.dwValue = (DWORD)((vol * 65535) / 100);
                mixerSetControlDetails((HMIXEROBJ)hMixer, &mcd, MIXER_SETCONTROLDETAILSF_VALUE);
            }
        }
        mixerClose(hMixer);
    }
}

void ApplyCurrentSchedule(bool force) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    if (!force) {
        if (st.wMinute == iLastMin) return;
        iLastMin = st.wMinute;
    }
    
    int currentDay = st.wDayOfWeek; 
    if (currentDay == 0) currentDay = 6; else currentDay -= 1; // 0=Mon..6=Sun
    
    wchar_t currentTime[10];
    swprintf(currentTime, 10, L"%02d:%02d", st.wHour, st.wMinute);

    int targetVol = -1;
    bool isDefault = false;
    wchar_t scheduleDays[100] = {0};

    int count = ListView_GetItemCount(hListView);
    for (int i = 0; i < count; i++) {
        wchar_t sDays[100], sFrom[10], sTo[10], sVol[10];
        ListView_GetItemText(hListView, i, 0, sDays, 100);
        ListView_GetItemText(hListView, i, 1, sFrom, 10);
        ListView_GetItemText(hListView, i, 2, sTo, 10);
        ListView_GetItemText(hListView, i, 3, sVol, 10);

        bool dayMatch = false;
        if (wcsstr(sDays, daysStr[currentDay]) != NULL) dayMatch = true;

        if (dayMatch) {
            if (force) {
                if (wcscmp(currentTime, sFrom) >= 0 && (wcscmp(sTo, L"--:--") == 0 || wcscmp(currentTime, sTo) < 0)) {
                    targetVol = _wtoi(sVol);
                    wcscpy(scheduleDays, sDays);
                    break;
                }
            } else {
                if (wcscmp(currentTime, sFrom) == 0) {
                    targetVol = _wtoi(sVol);
                    wcscpy(scheduleDays, sDays);
                    break;
                } else if (wcscmp(currentTime, sTo) == 0) {
                    wchar_t defVolStr[10];
                    GetWindowTextW(hDefVol, defVolStr, 10);
                    targetVol = _wtoi(defVolStr);
                    isDefault = true;
                    wcscpy(scheduleDays, sDays);
                    break;
                }
            }
        }
    }
    
    if (force && targetVol == -1) {
        wchar_t defVolStr[10];
        GetWindowTextW(hDefVol, defVolStr, 10);
        targetVol = _wtoi(defVolStr);
        isDefault = true;
    }

    if (targetVol != -1) {
        SetSysVolume(targetVol);
        wchar_t logMsg[200];
        if (force) {
            if (isDefault) swprintf(logMsg, 200, L"Принудительно возвращена громкость по умолчанию: %d%%", targetVol);
            else swprintf(logMsg, 200, L"Принудительно установлена громкость %d%% по расписанию \"%ls\"", targetVol, scheduleDays);
        } else {
            if (isDefault) swprintf(logMsg, 200, L"Громкость возвращена по умолчанию на %d%% (Конец правила: %ls)", targetVol, scheduleDays);
            else swprintf(logMsg, 200, L"Громкость изменена на %d%% по расписанию \"%ls\"", targetVol, scheduleDays);
        }
        LogEvent(logMsg);
    }
}

void UpdateClock() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    int curVol = GetSysVolume();
    wchar_t volBuf[50];
    swprintf(volBuf, 50, L"Текущая громкость: %d%%", curVol);
    SetWindowTextW(hCurVolLabel, volBuf);

    if (st.wSecond == iLastSec) return;
    iLastSec = st.wSecond;

    const wchar_t* days[] = {L"", L"Воскресенье", L"Понедельник", L"Вторник", L"Среда", L"Четверг", L"Пятница", L"Суббота"};
    int aiDay = st.wDayOfWeek + 1;

    wchar_t buf[100];
    swprintf(buf, 100, L"Текущее время: %ls, %02d:%02d:%02d", days[aiDay], st.wHour, st.wMinute, st.wSecond);
    SetWindowTextW(hTimeLabel, buf);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_COMMAND: {
            if (LOWORD(wParam) == IDC_AUTOSTART && HIWORD(wParam) == BN_CLICKED) {
                HKEY hKey;
                if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
                    if (SendMessage(hAutoStart, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                        wchar_t exePath[MAX_PATH];
                        GetModuleFileNameW(NULL, exePath, MAX_PATH);
                        RegSetValueExW(hKey, L"VolumeSchedule", 0, REG_SZ, (LPBYTE)exePath, (wcslen(exePath) + 1) * sizeof(wchar_t));
                    } else {
                        RegDeleteValueW(hKey, L"VolumeSchedule");
                    }
                    RegCloseKey(hKey);
                }
                return 0;
            } else if (LOWORD(wParam) == IDC_STARTMINIMIZED && HIWORD(wParam) == BN_CLICKED) {
                int minVal = SendMessage(hStartMinimized, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0;
                wchar_t minStr[10]; swprintf(minStr, 10, L"%d", minVal);
                WritePrivateProfileStringW(L"Settings", L"StartMinimized", minStr, szIniFile);
                return 0;
            } else if (LOWORD(wParam) == IDC_ALLDAY && HIWORD(wParam) == BN_CLICKED) {
                bool isChecked = (SendMessage(hAllDay, BM_GETCHECK, 0, 0) == BST_CHECKED);
                EnableWindow(hTimeFrom, !isChecked);
                EnableWindow(hTimeTo, !isChecked);
                return 0;
            } else if (LOWORD(wParam) == IDC_FORCERUN) {
                ApplyCurrentSchedule(true);
                return 0;
            } else if (LOWORD(wParam) == IDC_HIDE) {
                nid.cbSize = sizeof(NOTIFYICONDATAW);
                nid.hWnd = hwnd;
                nid.uID = 1;
                nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
                nid.uCallbackMessage = WM_TRAYICON;
                nid.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(102));
                wcscpy(nid.szTip, L"Управление громкостью");
                Shell_NotifyIconW(NIM_ADD, &nid);
                ShowWindow(hwnd, SW_HIDE);
            } else if (LOWORD(wParam) == IDC_DELETE) {
                int sel = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                if (sel != -1) ListView_DeleteItem(hListView, sel);
            } else if (LOWORD(wParam) == IDC_ADD) {
                wchar_t day[100] = {0};
                for (int i=0; i<7; i++) {
                    if (SendMessage(hDaysCB[i], BM_GETCHECK, 0, 0) == BST_CHECKED) {
                        if (wcslen(day) > 0) wcscat(day, L", ");
                        wcscat(day, daysStr[i]);
                    }
                }
                if (wcslen(day) == 0) {
                    MessageBoxW(hwnd, L"Выберите хотя бы один день недели!", L"Ошибка", MB_ICONERROR);
                    return 0;
                }

                wchar_t from[10], to[10], vol[10];
                GetWindowTextW(hVolume, vol, 10);
                
                bool allDay = (SendMessage(hAllDay, BM_GETCHECK, 0, 0) == BST_CHECKED);
                if (allDay) {
                    wcscpy(from, L"00:00");
                    wcscpy(to, L"--:--");
                } else {
                    GetWindowTextW(hTimeFrom, from, 10);
                    GetWindowTextW(hTimeTo, to, 10);
                    int hF, mF, hT, mT;
                    if (swscanf(from, L"%d:%d", &hF, &mF) != 2 || hF < 0 || hF > 23 || mF < 0 || mF > 59) {
                        MessageBoxW(hwnd, L"Неверный формат времени 'С' (используйте ЧЧ:ММ, 00-23:00-59).", L"Ошибка ввода", MB_ICONERROR);
                        return 0;
                    }
                    if (swscanf(to, L"%d:%d", &hT, &mT) != 2 || hT < 0 || hT > 23 || mT < 0 || mT > 59) {
                        MessageBoxW(hwnd, L"Неверный формат времени 'По' (используйте ЧЧ:ММ, 00-23:00-59).", L"Ошибка ввода", MB_ICONERROR);
                        return 0;
                    }
                    swprintf(from, 10, L"%02d:%02d", hF, mF);
                    swprintf(to, 10, L"%02d:%02d", hT, mT);
                }

                int count = ListView_GetItemCount(hListView);
                for (int i = 0; i < count; i++) {
                    wchar_t eDay[100], eFrom[10], eTo[10];
                    ListView_GetItemText(hListView, i, 0, eDay, 100);
                    ListView_GetItemText(hListView, i, 1, eFrom, 10);
                    ListView_GetItemText(hListView, i, 2, eTo, 10);
                    
                    bool dayOverlap = false;
                    for (int j=0; j<7; j++) {
                        if (wcsstr(day, daysStr[j]) && wcsstr(eDay, daysStr[j])) {
                            dayOverlap = true;
                            break;
                        }
                    }
                    
                    if (dayOverlap) {
                        bool overlap = false;
                        bool eInfinity = (wcscmp(eTo, L"--:--") == 0);
                        bool newInfinity = (wcscmp(to, L"--:--") == 0);

                        if (newInfinity && eInfinity) overlap = true;
                        else if (newInfinity) {
                            if (wcscmp(from, eTo) < 0) overlap = true; 
                        } else if (eInfinity) {
                            if (wcscmp(to, eFrom) > 0) overlap = true; 
                        } else {
                            if (wcscmp(from, eTo) < 0 && wcscmp(to, eFrom) > 0) overlap = true;
                        }

                        if (overlap) {
                            MessageBoxW(hwnd, L"Этот интервал пересекается с уже существующим правилом!", L"Ошибка", MB_ICONERROR);
                            return 0;
                        }
                    }
                }

                LVITEMW lvi = {0}; lvi.mask = LVIF_TEXT; lvi.iItem = count; lvi.iSubItem = 0; lvi.pszText = day;
                ListView_InsertItem(hListView, &lvi);
                ListView_SetItemText(hListView, count, 1, from);
                ListView_SetItemText(hListView, count, 2, to);
                ListView_SetItemText(hListView, count, 3, vol);
            } else if (LOWORD(wParam) == IDC_SAVE) {
                WritePrivateProfileStringW(L"Schedule", NULL, NULL, szIniFile); // Clear section
                int count = ListView_GetItemCount(hListView);
                wchar_t countStr[10]; swprintf(countStr, 10, L"%d", count);
                WritePrivateProfileStringW(L"Schedule", L"Count", countStr, szIniFile);
                for (int i=0; i<count; i++) {
                    wchar_t key[20], day[100], from[10], to[10], vol[10];
                    ListView_GetItemText(hListView, i, 0, day, 100);
                    ListView_GetItemText(hListView, i, 1, from, 10);
                    ListView_GetItemText(hListView, i, 2, to, 10);
                    ListView_GetItemText(hListView, i, 3, vol, 10);
                    swprintf(key, 20, L"Day%d", i); WritePrivateProfileStringW(L"Schedule", key, day, szIniFile);
                    swprintf(key, 20, L"From%d", i); WritePrivateProfileStringW(L"Schedule", key, from, szIniFile);
                    swprintf(key, 20, L"To%d", i); WritePrivateProfileStringW(L"Schedule", key, to, szIniFile);
                    swprintf(key, 20, L"Vol%d", i); WritePrivateProfileStringW(L"Schedule", key, vol, szIniFile);
                }
                wchar_t defv[10]; GetWindowTextW(hDefVol, defv, 10);
                WritePrivateProfileStringW(L"Settings", L"DefaultVol", defv, szIniFile);
            }
            return 0;
        }
        case WM_CREATE: {
            GetModuleFileNameW(NULL, szIniFile, MAX_PATH);
            wchar_t* p = wcsrchr(szIniFile, L'\\');
            if (p) { *p = 0; wcscat(szIniFile, L"\\settings.ini"); }

            HANDLE hIni = CreateFileW(szIniFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hIni == INVALID_HANDLE_VALUE) {
                hIni = CreateFileW(szIniFile, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hIni != INVALID_HANDLE_VALUE) {
                    unsigned char bom[] = { 0xFF, 0xFE };
                    DWORD bw;
                    WriteFile(hIni, bom, 2, &bw, NULL);
                }
            }
            if (hIni != INVALID_HANDLE_VALUE) CloseHandle(hIni);

            hListView = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 10, 10, 420, 120, hwnd, (HMENU)IDC_LISTVIEW, hInst, NULL);
            ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
            LVCOLUMNW lvc = {0}; lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            lvc.cx = 140; lvc.pszText = (LPWSTR)L"Дни"; ListView_InsertColumn(hListView, 0, &lvc);
            lvc.cx = 60; lvc.pszText = (LPWSTR)L"С"; ListView_InsertColumn(hListView, 1, &lvc);
            lvc.cx = 60; lvc.pszText = (LPWSTR)L"По"; ListView_InsertColumn(hListView, 2, &lvc);
            lvc.cx = 100; lvc.pszText = (LPWSTR)L"Громкость (%)"; ListView_InsertColumn(hListView, 3, &lvc);

            for(int i=0; i<7; i++) {
                hDaysCB[i] = CreateWindowExW(0, L"BUTTON", daysStr[i], WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 10 + i*45, 140, 45, 20, hwnd, (HMENU)(IDC_DAY_MON + i), hInst, NULL);
            }

            CreateWindowExW(0, L"STATIC", L"С:", WS_CHILD | WS_VISIBLE, 10, 173, 20, 20, hwnd, NULL, hInst, NULL);
            hTimeFrom = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"09:00", WS_CHILD | WS_VISIBLE, 30, 170, 50, 22, hwnd, (HMENU)IDC_TIMEFROM, hInst, NULL);
            CreateWindowExW(0, L"STATIC", L"По:", WS_CHILD | WS_VISIBLE, 90, 173, 25, 20, hwnd, NULL, hInst, NULL);
            hTimeTo = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"18:00", WS_CHILD | WS_VISIBLE, 115, 170, 50, 22, hwnd, (HMENU)IDC_TIMETO, hInst, NULL);
            
            hAllDay = CreateWindowExW(0, L"BUTTON", L"Весь день", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 180, 170, 90, 20, hwnd, (HMENU)IDC_ALLDAY, hInst, NULL);
            
            CreateWindowExW(0, L"STATIC", L"Громкость:", WS_CHILD | WS_VISIBLE, 280, 173, 70, 20, hwnd, NULL, hInst, NULL);
            hVolume = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"50", WS_CHILD | WS_VISIBLE | ES_NUMBER, 350, 170, 40, 22, hwnd, (HMENU)IDC_VOLUME, hInst, NULL);

            CreateWindowExW(0, L"BUTTON", L"Добавить", WS_CHILD | WS_VISIBLE, 10, 210, 90, 30, hwnd, (HMENU)IDC_ADD, hInst, NULL);
            CreateWindowExW(0, L"BUTTON", L"Сохранить", WS_CHILD | WS_VISIBLE, 110, 210, 90, 30, hwnd, (HMENU)IDC_SAVE, hInst, NULL);
            CreateWindowExW(0, L"BUTTON", L"Удалить", WS_CHILD | WS_VISIBLE, 210, 210, 90, 30, hwnd, (HMENU)IDC_DELETE, hInst, NULL);
            CreateWindowExW(0, L"BUTTON", L"Применить", WS_CHILD | WS_VISIBLE, 310, 210, 110, 30, hwnd, (HMENU)IDC_FORCERUN, hInst, NULL);

            CreateWindowExW(0, L"STATIC", L"Громкость по умолчанию:", WS_CHILD | WS_VISIBLE, 10, 263, 170, 20, hwnd, NULL, hInst, NULL);
            hDefVol = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"100", WS_CHILD | WS_VISIBLE | ES_NUMBER, 180, 260, 50, 22, hwnd, (HMENU)IDC_DEFVOL, hInst, NULL);

            hAutoStart = CreateWindowExW(0, L"BUTTON", L"Запуск с Windows", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 250, 250, 150, 20, hwnd, (HMENU)IDC_AUTOSTART, hInst, NULL);
            hStartMinimized = CreateWindowExW(0, L"BUTTON", L"Запуск свернутым", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 250, 270, 150, 20, hwnd, (HMENU)IDC_STARTMINIMIZED, hInst, NULL);

            CreateWindowExW(0, L"BUTTON", L"Скрыть в трей", WS_CHILD | WS_VISIBLE, 10, 300, 120, 30, hwnd, (HMENU)IDC_HIDE, hInst, NULL);
            hTimeLabel = CreateWindowExW(0, L"STATIC", L"Текущее время:", WS_CHILD | WS_VISIBLE, 140, 300, 300, 20, hwnd, (HMENU)IDC_TIMELABEL, hInst, NULL);
            hCurVolLabel = CreateWindowExW(0, L"STATIC", L"Текущая громкость: --%", WS_CHILD | WS_VISIBLE, 140, 320, 300, 20, hwnd, (HMENU)IDC_CURVOL, hInst, NULL);

            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            EnumChildWindows(hwnd, EnumChildProc, (LPARAM)hFont);

            SetTimer(hwnd, 1, 1000, NULL);

            // Load settings
            wchar_t countStr[10];
            GetPrivateProfileStringW(L"Schedule", L"Count", L"0", countStr, 10, szIniFile);
            int count = _wtoi(countStr);
            for(int i=0; i<count; i++) {
                wchar_t key[20], day[100], from[10], to[10], vol[10];
                swprintf(key, 20, L"Day%d", i); GetPrivateProfileStringW(L"Schedule", key, L"", day, 100, szIniFile);
                swprintf(key, 20, L"From%d", i); GetPrivateProfileStringW(L"Schedule", key, L"", from, 10, szIniFile);
                swprintf(key, 20, L"To%d", i); GetPrivateProfileStringW(L"Schedule", key, L"", to, 10, szIniFile);
                swprintf(key, 20, L"Vol%d", i); GetPrivateProfileStringW(L"Schedule", key, L"", vol, 10, szIniFile);
                LVITEMW lvi = {0}; lvi.mask = LVIF_TEXT; lvi.iItem = i; lvi.iSubItem = 0; lvi.pszText = day;
                ListView_InsertItem(hListView, &lvi);
                ListView_SetItemText(hListView, i, 1, from);
                ListView_SetItemText(hListView, i, 2, to);
                ListView_SetItemText(hListView, i, 3, vol);
            }
            wchar_t defv[10];
            GetPrivateProfileStringW(L"Settings", L"DefaultVol", L"100", defv, 10, szIniFile);
            SetWindowTextW(hDefVol, defv);

            int minVal = GetPrivateProfileIntW(L"Settings", L"StartMinimized", 0, szIniFile);
            SendMessage(hStartMinimized, BM_SETCHECK, minVal ? BST_CHECKED : BST_UNCHECKED, 0);

            HKEY hKey;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                wchar_t val[MAX_PATH];
                DWORD size = sizeof(val);
                if (RegQueryValueExW(hKey, L"VolumeSchedule", NULL, NULL, (LPBYTE)val, &size) == ERROR_SUCCESS) {
                    SendMessage(hAutoStart, BM_SETCHECK, BST_CHECKED, 0);
                } else {
                    SendMessage(hAutoStart, BM_SETCHECK, BST_UNCHECKED, 0);
                }
                RegCloseKey(hKey);
            }

            return 0;
        }
        case WM_TRAYICON:
            if (lParam == WM_LBUTTONDBLCLK || lParam == WM_LBUTTONDOWN) {
                Shell_NotifyIconW(NIM_DELETE, &nid);
                ShowWindow(hwnd, SW_SHOW);
                SetForegroundWindow(hwnd);
            } else if (lParam == WM_RBUTTONUP) {
                HMENU hMenu = CreatePopupMenu();
                InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, 2001, L"Открыть");
                InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, 2002, L"Выход");
                
                POINT pt;
                GetCursorPos(&pt);
                SetForegroundWindow(hwnd);
                int cmd = TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN | TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);
                
                if (cmd == 2001) {
                    Shell_NotifyIconW(NIM_DELETE, &nid);
                    ShowWindow(hwnd, SW_SHOW);
                    SetForegroundWindow(hwnd);
                } else if (cmd == 2002) {
                    Shell_NotifyIconW(NIM_DELETE, &nid);
                    PostQuitMessage(0);
                }
            }
            return 0;
        case WM_TIMER:
            UpdateClock();
            ApplyCurrentSchedule(false);
            return 0;
        case WM_CLOSE:
            SendMessage(hwnd, WM_COMMAND, IDC_HIDE, 0);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR pCmdLine, int nCmdShow) {
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"VolumeXPMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"Программа уже запущена!", L"Ошибка", MB_ICONERROR);
        return 0;
    }

    hInst = hInstance;
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    const wchar_t CLASS_NAME[] = L"VolumeScheduleClass";
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    RegisterClassW(&wc);

    hWndMain = CreateWindowExW(
        0, CLASS_NAME, L"Управление громкостью",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 460, 390,
        NULL, NULL, hInstance, NULL
    );

    if (hWndMain == NULL) return 0;
    
    // Применяем расписание при старте программы
    SendMessage(hWndMain, WM_COMMAND, IDC_FORCERUN, 0);

    wchar_t iniP[MAX_PATH];
    GetModuleFileNameW(NULL, iniP, MAX_PATH);
    wchar_t* ptr = wcsrchr(iniP, L'\\');
    if (ptr) { *ptr = 0; wcscat(iniP, L"\\settings.ini"); }
    int startMin = GetPrivateProfileIntW(L"Settings", L"StartMinimized", 0, iniP);
    
    if (startMin) {
        SendMessage(hWndMain, WM_COMMAND, IDC_HIDE, 0);
    } else {
        ShowWindow(hWndMain, nCmdShow);
    }

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    return 0;
}
