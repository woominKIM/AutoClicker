#define _CRT_SECURE_NO_WARNINGS
#include "resource.h"
#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <chrono>
#include <thread>
#include <ctime>

HINSTANCE curInstance;

using namespace std;
static int curWidth = 480;
static int curHeight = 150;

static HWND hwnd;
static HWND hwndTimeInputWindow = NULL;  // �ϰ� �Է� â �ڵ�
static HWND hwndInputHour, hwndInputMinute, hwndInputSecond, hwndInputMillisecond; // �Է� �ʵ� �ڵ�


static bool timeBulkChange = false, coordinateBulkChange = false;
static int bulkHour = 0, bulkMinute = 0, bulkSecond = 0, bulkMillisecond = 0, bulkX = 0, bulkY = 0;

static wchar_t curKeyName[64];
static int curKeyCode = 0;

struct ClickInput {
	int x, y;
	int hour, minute, second, millisecond;
	bool isKeyPress;
	int keyCode;
};

void ShowError(HWND hwnd, const wstring& message) {
	MessageBox(hwnd, message.c_str(), L"�Է� ����", MB_OK | MB_ICONERROR);
}

bool ValidateTime(HWND hwnd, int hour, int minute, int second, int millisecond) {
	if (hour < 0 || hour > 23) {
		ShowError(hwnd, L"�ð� ���� 0���� 23 ���̿��� �մϴ�.");
		return false;
	}
	if (minute < 0 || minute > 59) {
		ShowError(hwnd, L"�� ���� 0���� 59 ���̿��� �մϴ�.");
		return false;
	}
	if (second < 0 || second > 59) {
		ShowError(hwnd, L"�� ���� 0���� 59 ���̿��� �մϴ�.");
		return false;
	}
	if (millisecond < 0 || millisecond > 999) {
		ShowError(hwnd, L"�и��� ���� 0���� 999 ���̿��� �մϴ�.");
		return false;
	}
	return true;
}

bool ValidateCoordinate(HWND hwnd, int x, int y) {
	if (x < 0 || y < 0) {
		ShowError(hwnd, L"��ǥ ���� 0 �̻��� ���ڿ��� �մϴ�.");
		return false;
	}
	if (x > 9999 || y > 9999) {
		ShowError(hwnd, L"��ǥ ���� 0~9999 ���̷� �Է����ּ���.");
		return false;
	}
	return true;
}

INT_PTR CALLBACK KeyInputDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static int* outputKeyCode;
	static HWND hwndSetKeyButton;
	hwndSetKeyButton = GetDlgItem(hwndDlg, IDC_KEY_SET); // ��ǥ ���� ��ư

	switch (message) {
	case WM_INITDIALOG:
		outputKeyCode = reinterpret_cast<int*>(lParam); // ���޹��� Ű �ڵ带 ������ ������
		EnableWindow(GetDlgItem(hwndDlg, IDOK), FALSE); // Ȯ�ι�ư ��Ȱ��ȭ
		return (INT_PTR)TRUE;

	case WM_COMMAND:

		if (LOWORD(wParam) == IDOK) {
			EndDialog(hwndDlg, IDOK);
			return (INT_PTR)TRUE;
		}
		else if (LOWORD(wParam) == IDCANCEL) {
			EndDialog(hwndDlg, IDCANCEL);
			return (INT_PTR)TRUE;
		}
		else if (LOWORD(wParam) == IDC_KEY_SET) {  // "Ű ����" ��ư Ŭ�� ��
			// ��ư �ؽ�Ʈ ����
			SetWindowText(hwndSetKeyButton, L"�Է� �����");

			// Ű �Է��� ��ٸ�
			while (true) {
				if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {  // ���콺 ���ʹ�ư ����
					SetWindowText(hwndSetKeyButton, L"Ű �Է�");
					break;
				}
				for (int keyCode = 0x01; keyCode <= 0xFE; ++keyCode) {
					SHORT keyState = GetAsyncKeyState(keyCode);
					if (	keyState & 0x8000) { // Ű�� ���� �ִ� �������� Ȯ��
						UINT scanCode = MapVirtualKey(keyCode, MAPVK_VK_TO_VSC);
						GetKeyNameText((scanCode << 16), curKeyName, sizeof(curKeyName) / sizeof(wchar_t));
						SetWindowText(hwndSetKeyButton, curKeyName);
						*outputKeyCode = static_cast<int>(wParam);
						curKeyCode = keyCode;
						EnableWindow(GetDlgItem(hwndDlg, IDOK), TRUE); // Ȯ�ι�ư Ȱ��ȭ
						return (INT_PTR)FALSE;
					}
				}
			}
		}
		break;

	case WM_CLOSE:
		EndDialog(hwndDlg, IDCANCEL); // ��� �� �ݱ�
		return (INT_PTR)TRUE;
	}
	return (INT_PTR)FALSE;
}

INT_PTR CALLBACK TimeBulkChangeDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static HWND hwndHourInput, hwndMinuteInput, hwndSecondInput, hwndMillisecondInput;

	switch (message) {
	case WM_INITDIALOG:
		hwndHourInput = GetDlgItem(hwndDlg, IDC_HOUR);
		hwndMinuteInput = GetDlgItem(hwndDlg, IDC_MINUTE);
		hwndSecondInput = GetDlgItem(hwndDlg, IDC_SECOND);
		hwndMillisecondInput = GetDlgItem(hwndDlg, IDC_MILLISECOND);

		// �ý��� �ð� �����ͼ� �Է� �ʵ忡 ����
		SYSTEMTIME localTime;
		GetLocalTime(&localTime);
		SetWindowText(hwndHourInput, to_wstring(localTime.wHour).c_str());
		SetWindowText(hwndMinuteInput, to_wstring(localTime.wMinute).c_str());
		SetWindowText(hwndSecondInput, to_wstring(localTime.wSecond).c_str());
		SetWindowText(hwndMillisecondInput, L"0");
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) {  // "����" ��ư Ŭ�� ��
			wchar_t buffer[100];

			// �� �ʵ忡�� �� ��������
			GetWindowText(hwndHourInput, buffer, sizeof(buffer) / sizeof(wchar_t));
			int hour = wcstol(buffer, NULL, 10);

			GetWindowText(hwndMinuteInput, buffer, sizeof(buffer) / sizeof(wchar_t));
			int minute = wcstol(buffer, NULL, 10);

			GetWindowText(hwndSecondInput, buffer, sizeof(buffer) / sizeof(wchar_t));
			int second = wcstol(buffer, NULL, 10);

			GetWindowText(hwndMillisecondInput, buffer, sizeof(buffer) / sizeof(wchar_t));
			int millisecond = wcstol(buffer, NULL, 10);

			if (ValidateTime(hwndDlg, hour, minute, second, millisecond)) {
				bulkHour = hour;
				bulkMinute = minute;
				bulkSecond = second;
				bulkMillisecond = millisecond;
				EndDialog(hwndDlg, IDOK);
				timeBulkChange = true;
			}
			return (INT_PTR)TRUE;
		}
		else if (LOWORD(wParam) == IDCANCEL) {  // "���" ��ư Ŭ�� ��
			EndDialog(hwndDlg, IDCANCEL);
			return (INT_PTR)TRUE;
		}
		break;
	}

	return (INT_PTR)FALSE;
}

INT_PTR CALLBACK CoordinateBulkChangeDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static HWND hwndXInput, hwndYInput;
	static HWND hwndSetCoordinateButton;

	switch (message) {

	case WM_INITDIALOG:
		hwndXInput = GetDlgItem(hwndDlg, IDC_X);  // X ��ǥ �Է� �ʵ�
		hwndYInput = GetDlgItem(hwndDlg, IDC_Y);  // Y ��ǥ �Է� �ʵ�
		hwndSetCoordinateButton = GetDlgItem(hwndDlg, IDC_COORDINATE_SET); // ��ǥ ���� ��ư

		// ���� �� (���⼭ bulkX, bulkY�� �⺻���� ������ �ִٰ� ����)
		SetWindowText(hwndXInput, to_wstring(bulkX).c_str());
		SetWindowText(hwndYInput, to_wstring(bulkY).c_str());

		return (INT_PTR)TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) {  // "����" ��ư Ŭ�� ��
			wchar_t buffer[100];
			// X ��ǥ ��������
			GetWindowText(hwndXInput, buffer, sizeof(buffer) / sizeof(wchar_t));
			int x = wcstol(buffer, NULL, 10);

			GetWindowText(hwndYInput, buffer, sizeof(buffer) / sizeof(wchar_t));
			int y = wcstol(buffer, NULL, 10);

			if (ValidateCoordinate(hwndDlg, x, y)) {
				bulkX = x;
				bulkY = y;
				EndDialog(hwndDlg, IDOK);
				coordinateBulkChange = true;
			}
		}
		else if (LOWORD(wParam) == IDCANCEL) {  // "���" ��ư Ŭ�� ��
			EndDialog(hwndDlg, IDCANCEL);
			return (INT_PTR)TRUE;
		}
		else if (LOWORD(wParam) == IDC_COORDINATE_SET) {  // "��ǥ ����" ��ư Ŭ�� ��

			// ��ư �ؽ�Ʈ�� "ZŰ �Է�"���� ����
			SetWindowText(hwndSetCoordinateButton, L"ZŰ �Է�");

			// Z Ű �Է��� ��ٸ�
			while (true) {
				if (GetAsyncKeyState('Z') & 0x8000) {  // Z Ű�� ������
					POINT p;
					GetCursorPos(&p);

					// ��ǥ�� �Է� �ʵ忡 ����
					SetWindowText(hwndXInput, to_wstring(p.x).c_str());
					SetWindowText(hwndYInput, to_wstring(p.y).c_str());

					// ��ư �ؽ�Ʈ�� �ٽ� "��ǥ ����"���� ����
					SetWindowText(hwndSetCoordinateButton, L"��ǥ ����");
					break;
				}
				if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {  // ���콺 ���ʹ�ư ����
					SetWindowText(hwndSetCoordinateButton, L"��ǥ ����");
					break;
				}
				this_thread::sleep_for(chrono::milliseconds(100));
			}
		}
		break;

	}

	return (INT_PTR)FALSE;
}

// ������ ���ν���
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	static vector<ClickInput> clicks;
	static HWND hwndAddClickButton, hwndStartMacroButton, hwndRefreshButton;
	static HWND hwndHintXY, hwndHintK, hwndHintD, hwndHintHour, hwndHintMinute, hwndHintSecond, hwndHintMilliSecond;
	static HWND hwndTimeBulkChangeButton, hwndCoordinateBulkChangeButton;
	static vector<HWND> hwndX, hwndY, hwndHour, hwndMinute, hwndSecond, hwndMillisecond, hwndSetPosButton;
	static vector<HWND> hwndKeyPressCheckBox, hwndKeyCodeInput;
	static vector<HWND> hwndDeleteButton;
	static vector<int> realKeyCode;
	static HFONT hFont;
	static int currentClickCount = 0;

	switch (msg) {  // ���⼭���� switch �� ����
	case WM_CREATE: {
		hwndStartMacroButton = CreateWindow(L"BUTTON", L"����", WS_VISIBLE | WS_CHILD, 20, 20, 60, 30, hwnd, (HMENU)2, NULL, NULL);
		hwndTimeBulkChangeButton = CreateWindow(L"BUTTON", L"�ð� �ϰ� ����", WS_VISIBLE | WS_CHILD, 90, 20, 90, 30, hwnd, (HMENU)4, NULL, NULL);
		hwndCoordinateBulkChangeButton = CreateWindow(L"BUTTON", L"��ǥ �ϰ� ����", WS_VISIBLE | WS_CHILD, 190, 20, 90, 30, hwnd, (HMENU)5, NULL, NULL);
		hwndAddClickButton = CreateWindow(L"BUTTON", L"���� �߰�", WS_VISIBLE | WS_CHILD, 305, 20, 60, 30, hwnd, (HMENU)1, NULL, NULL);
		hwndRefreshButton = CreateWindow(L"BUTTON", L"�ʱ�ȭ", WS_VISIBLE | WS_CHILD, 375, 20, 60, 30, hwnd, (HMENU)3, NULL, NULL);


		hwndHintHour = CreateWindow(L"STATIC", L"��", WS_VISIBLE | WS_CHILD, 20, 65, 20, 20, hwnd, NULL, NULL, NULL);
		hwndHintMinute = CreateWindow(L"STATIC", L"��", WS_VISIBLE | WS_CHILD, 45, 65, 20, 20, hwnd, NULL, NULL, NULL);
		hwndHintSecond = CreateWindow(L"STATIC", L"��", WS_VISIBLE | WS_CHILD, 70, 65, 20, 20, hwnd, NULL, NULL, NULL);
		hwndHintMilliSecond = CreateWindow(L"STATIC", L"ms", WS_VISIBLE | WS_CHILD | SS_CENTER, 95, 65, 30, 20, hwnd, NULL, NULL, NULL);
		hwndHintXY = CreateWindow(L"STATIC", L"��ǥ (X,Y)", WS_VISIBLE | WS_CHILD | SS_CENTER, 140, 65, 170, 20, hwnd, NULL, NULL, NULL);
		hwndHintK = CreateWindow(L"STATIC", L"Ű �Է�", WS_VISIBLE | WS_CHILD | SS_CENTER, 325, 65, 75, 20, hwnd, NULL, NULL, NULL);
		hwndHintK = CreateWindow(L"STATIC", L"��", WS_VISIBLE | WS_CHILD | SS_CENTER, 415, 65, 20, 20, hwnd, NULL, NULL, NULL);
		
		//���� ����
		EnableWindow(hwndStartMacroButton, FALSE);
		EnableWindow(hwndTimeBulkChangeButton, FALSE);
		EnableWindow(hwndCoordinateBulkChangeButton, FALSE);

		//��Ʈ ����
		hFont = CreateFont(
			-MulDiv(9, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72), // ����Ʈ ũ�⸦ �ȼ��� ��ȯ
			0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial"
		);
		SendMessage(hwndAddClickButton, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessage(hwndStartMacroButton, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessage(hwndRefreshButton, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessage(hwndTimeBulkChangeButton, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessage(hwndCoordinateBulkChangeButton, WM_SETFONT, (WPARAM)hFont, TRUE);

		// â ����� ������� ����
		HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255)); // ���
		SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBrush);
		break;
	}
	case WM_COMMAND: {
		if (LOWORD(wp) == 1) {  // Add Click ��ư Ŭ�� ��

			int yOffset = 95 + currentClickCount * 28;  // ��ư�� ���� �� ��Ÿ���� �Է¶��� ��ġ

			SYSTEMTIME localTime;
			GetLocalTime(&localTime);

			hwndHour.push_back(CreateWindow(L"EDIT", to_wstring(localTime.wHour).c_str(), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 20, yOffset, 20, 20, hwnd, NULL, NULL, NULL));
			hwndMinute.push_back(CreateWindow(L"EDIT", to_wstring(localTime.wMinute).c_str(), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 45, yOffset, 20, 20, hwnd, NULL, NULL, NULL));
			hwndSecond.push_back(CreateWindow(L"EDIT", to_wstring(localTime.wSecond).c_str(), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 70, yOffset, 20, 20, hwnd, NULL, NULL, NULL));
			hwndMillisecond.push_back(CreateWindow(L"EDIT", L"0", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 95, yOffset, 30, 20, hwnd, NULL, NULL, NULL));

			hwndX.push_back(CreateWindow(L"EDIT", L"0", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 140, yOffset, 40, 20, hwnd, NULL, NULL, NULL));  // �⺻�� 0
			hwndY.push_back(CreateWindow(L"EDIT", L"0", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 185, yOffset, 40, 20, hwnd, NULL, NULL, NULL));  // �⺻�� 0
			hwndSetPosButton.push_back(CreateWindow(L"BUTTON", L"��ǥ ����", WS_VISIBLE | WS_CHILD, 230, yOffset, 80, 20, hwnd, reinterpret_cast<HMENU>(static_cast<ULONG_PTR>(6 + currentClickCount)), NULL, NULL));

			hwndKeyPressCheckBox.push_back(CreateWindow(L"BUTTON", L"off", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 325, yOffset, 40, 20, hwnd, reinterpret_cast<HMENU>(1000 + currentClickCount), NULL, NULL));
			hwndKeyCodeInput.push_back(CreateWindow(L"STATIC", L"", WS_VISIBLE | WS_CHILD, 370, yOffset, 30, 20, hwnd, NULL, NULL, NULL));
			hwndDeleteButton.push_back(CreateWindow(L"BUTTON", L"��", WS_VISIBLE | WS_CHILD, 415, yOffset, 20, 20, hwnd, reinterpret_cast<HMENU>(2000 + currentClickCount), NULL, NULL));

			realKeyCode.push_back(0);
			currentClickCount++;

			// â ũ�� �������� ���� (�ּ� ���� 160, �ʿ��� ��ŭ �÷���)
			int newHeight = curHeight + currentClickCount * 28;
			SetWindowPos(hwnd, NULL, 0, 0, curWidth, newHeight, SWP_NOZORDER | SWP_NOMOVE);
			
			//��ư Ȱ��ȭ
			EnableWindow(hwndStartMacroButton, TRUE);
			EnableWindow(hwndTimeBulkChangeButton, TRUE);
			EnableWindow(hwndCoordinateBulkChangeButton, TRUE);
			
			// â ���� ����
			InvalidateRect(hwnd, NULL, TRUE);
			UpdateWindow(hwnd);
		}
		else if (LOWORD(wp) == 2) {  // Start Macro ��ư Ŭ�� ��
			clicks.clear();

			// �Էµ� ������ Ŭ�� ��Ͽ� �߰�
			for (int i = 0; i < currentClickCount; ++i) {
				ClickInput click;
				wchar_t buffer[100];

				GetWindowText(hwndX[i], buffer, sizeof(buffer) / sizeof(wchar_t));
				click.x = wcstol(buffer, NULL, 10);
				GetWindowText(hwndY[i], buffer, sizeof(buffer) / sizeof(wchar_t));
				click.y = wcstol(buffer, NULL, 10);

				if (!ValidateCoordinate(hwnd, click.x, click.y)) {
					return 0;  // ���� �޽��� ��� �� ���� �ߴ�
				}

				GetWindowText(hwndHour[i], buffer, sizeof(buffer) / sizeof(wchar_t));
				click.hour = wcstol(buffer, NULL, 10);
				GetWindowText(hwndMinute[i], buffer, sizeof(buffer) / sizeof(wchar_t));
				click.minute = wcstol(buffer, NULL, 10);
				GetWindowText(hwndSecond[i], buffer, sizeof(buffer) / sizeof(wchar_t));
				click.second = wcstol(buffer, NULL, 10);
				GetWindowText(hwndMillisecond[i], buffer, sizeof(buffer) / sizeof(wchar_t));
				click.millisecond = wcstol(buffer, NULL, 10);

				if (!ValidateTime(hwnd, click.hour, click.minute, click.second, click.millisecond)) {
					return 0;  // ���� �޽��� ��� �� ���� �ߴ�
				}

				if (SendMessage(hwndKeyPressCheckBox[i], BM_GETCHECK, 0, 0) == BST_CHECKED) {
					click.isKeyPress = true;
					click.keyCode = realKeyCode[i];
				}
				else {
					click.isKeyPress = false;
				}
				clicks.push_back(click);
			}

			// ��ũ�� ����
			for (const auto& click : clicks) {
				SYSTEMTIME localTime;
				GetLocalTime(&localTime);
				int currentHour = localTime.wHour;
				int currentMinute = localTime.wMinute;
				int currentSecond = localTime.wSecond;
				int currentMillisecond = localTime.wMilliseconds;

				long long nowMs = (currentHour * 3600000) + (currentMinute * 60000) + (currentSecond * 1000) + currentMillisecond;
				long long targetTimeInMs = (click.hour * 3600000) + (click.minute * 60000) + (click.second * 1000) + click.millisecond;

				while (nowMs < targetTimeInMs) {
					//this_thread::sleep_for(chrono::milliseconds(10));
					GetLocalTime(&localTime);
					currentHour = localTime.wHour;
					currentMinute = localTime.wMinute;
					currentSecond = localTime.wSecond;
					currentMillisecond = localTime.wMilliseconds;
					nowMs = (currentHour * 3600000) + (currentMinute * 60000) + (currentSecond * 1000) + currentMillisecond;
				}

				if (click.isKeyPress) {
					static HWND hWNd;
					// Ű �Է� ó��
					keybd_event(click.keyCode, 0, 0, 0);  // Ű ����
					keybd_event(click.keyCode, 0, KEYEVENTF_KEYUP, 0); // Ű ��
				}
				else {
					SetCursorPos(click.x, click.y);
					mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
					mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
				}
			}
		}
		else if (LOWORD(wp) == 3) {  // Refresh ��ư Ŭ�� ��
			clicks.clear();
			currentClickCount = 0;

			// ��� �Է� �ʵ带 ����
			for (int i = 0; i < currentClickCount; ++i) {
				DestroyWindow(hwndX[i]);
				DestroyWindow(hwndY[i]);
				DestroyWindow(hwndHour[i]);
				DestroyWindow(hwndMinute[i]);
				DestroyWindow(hwndSecond[i]);
				DestroyWindow(hwndMillisecond[i]);
				DestroyWindow(hwndSetPosButton[i]);
			}

			// â ũ�� �ּ�ȭ �� ���α׷� �����
			HWND hwndCurrent = hwnd;
			PostMessage(hwndCurrent, WM_CLOSE, 0, 0);

			STARTUPINFO si = {};
			PROCESS_INFORMATION pi = {};
			TCHAR szPath[MAX_PATH];
			GetModuleFileName(NULL, szPath, MAX_PATH);  // ���� ���� ���� ���α׷� ��� ��������

			if (CreateProcess(szPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
				// ���μ����� ���������� �����Ǹ� �ڵ� �ݱ�
				CloseHandle(pi.hProcess);  // ���μ��� �ڵ� �ݱ�
				CloseHandle(pi.hThread);   // ������ �ڵ� �ݱ�
			}
			else {
				// CreateProcess ���� ó�� (�ʿ��)
				MessageBox(hwnd, L"���μ��� ���� ����", L"����", MB_OK | MB_ICONERROR);
			}
		}

		// "�ð� �ϰ�����" ��ư Ŭ�� ��
		else if (LOWORD(wp) == 4) {
			// ���̾�α� ����
			DialogBox(curInstance, MAKEINTRESOURCE(IDD_TIME_BULK_CHANGE), hwnd, TimeBulkChangeDlgProc);

			// ���̾�α׿��� �ð��� ����Ǿ����� ���� �����쿡�� ����� �ð��� ����� �� ����
			// ���� ���, ����� �ð��� ǥ���ϴ� �κ��� �߰��� �� �ֽ��ϴ�.
			wstring changedTime = to_wstring(bulkHour) + L":" + to_wstring(bulkMinute) + L":" + to_wstring(bulkSecond) + L"." + to_wstring(bulkMillisecond);
			
			if (timeBulkChange) {
				for (int i = 0; i < currentClickCount; ++i) {
					SetWindowText(hwndHour[i], to_wstring(bulkHour).c_str());
					SetWindowText(hwndMinute[i], to_wstring(bulkMinute).c_str());
					SetWindowText(hwndSecond[i], to_wstring(bulkSecond).c_str());
					SetWindowText(hwndMillisecond[i], to_wstring(bulkMillisecond).c_str());
				}
				timeBulkChange = false;
			}
		}

		// '��ǥ �ϰ�����' ��ư Ŭ�� ��
		else if (LOWORD(wp) == 5) {
			// ���̾�α� ����
			DialogBox(curInstance, MAKEINTRESOURCE(IDD_COORDINATE_BULK_CHANGE), hwnd, CoordinateBulkChangeDlgProc);

			// ���̾�α׿��� ��ǥ�� ����Ǿ����� ���� �����쿡�� ����� ��ǥ�� ����� �� ����
			// ���� ���, ����� ��ǥ�� ��� �Է� �ʵ忡 �����ϴ� �κ��� �߰��մϴ�.

			if (coordinateBulkChange) {
				for (int i = 0; i < currentClickCount; ++i) {
					// ����� ��ǥ�� �� �Է� �ʵ忡 ����
					SetWindowText(hwndX[i], to_wstring(bulkX).c_str());
					SetWindowText(hwndY[i], to_wstring(bulkY).c_str());
				}
				coordinateBulkChange = false;
			}
		}

		// �� Ŭ�� ������ Set Pos ��ư Ŭ�� ��, Z Ű �Է��� ��ٸ��� ���� �̺�Ʈ ó��
		else if (LOWORD(wp) >= 6 && LOWORD(wp) < 1000) {
			int index = LOWORD(wp) - 6;

			// ��ư �ؽ�Ʈ�� "ZŰ �Է�"���� ����
			SetWindowText(hwndSetPosButton[index], L"ZŰ �Է�");

			// Z Ű �Է��� ��ٸ�
			while (true) {
				if (GetAsyncKeyState('Z') & 0x8000) {  // Z Ű�� ������
					POINT p;
					GetCursorPos(&p);

					// ��ǥ�� �Է� �ʵ忡 ����
					SetWindowText(hwndX[index], to_wstring(p.x).c_str());
					SetWindowText(hwndY[index], to_wstring(p.y).c_str());

					// ��ư �ؽ�Ʈ�� �ٽ� "��ǥ ����"���� ����
					SetWindowText(hwndSetPosButton[index], L"��ǥ ����");
					break;
				}
				if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {  // ���콺 ���ʹ�ư ����
					SetWindowText(hwndSetPosButton[index], L"��ǥ ����");
					break;
				}
			}
		}

		else if (LOWORD(wp) >= 1000 && LOWORD(wp) < 2000) { // Ű �Է� üũ�ڽ� ID ����
			int index = LOWORD(wp) - 1000; // üũ�ڽ��� ���� �ε���
			UINT keyCode = 0;
			if (SendMessage(hwndKeyPressCheckBox[index], BM_GETCHECK, 0, 0) == BST_CHECKED) {

				// Ű �Է� ���̾�α� ȣ��
				if (DialogBoxParam(curInstance, MAKEINTRESOURCE(IDD_KEY_INPUT), hwnd, KeyInputDlgProc, reinterpret_cast<LPARAM>(&keyCode)) == IDOK) {
					
					//�Էµ� Ű �̸� ǥ��
					SetWindowText(hwndKeyPressCheckBox[index], L"on");
					SetWindowTextW(hwndKeyCodeInput[index], curKeyName);
					realKeyCode[index] = curKeyCode;

					// ��ǥ ���� ��ư �� �ʵ� ��Ȱ��ȭ
					EnableWindow(hwndX[index], FALSE); // X ��ǥ �ʵ� ��Ȱ��ȭ
					EnableWindow(hwndY[index], FALSE); // Y ��ǥ �ʵ� ��Ȱ��ȭ
					EnableWindow(hwndSetPosButton[index], FALSE); // ��ǥ ���� ��ư ��Ȱ��ȭ
				}
				else {
					// üũ�ڽ� ���� ���
					SendMessage(hwndKeyPressCheckBox[index], BM_SETCHECK, BST_UNCHECKED, 0);
				}
			}
			else {
				// üũ�ڽ��� �����Ǹ� ��ǥ ������ �ٽ� Ȱ��ȭ
				SetWindowText(hwndKeyPressCheckBox[index], L"off");
				EnableWindow(hwndX[index], TRUE); // X ��ǥ �ʵ� Ȱ��ȭ
				EnableWindow(hwndY[index], TRUE); // Y ��ǥ �ʵ� Ȱ��ȭ
				EnableWindow(hwndSetPosButton[index], TRUE); // ��ǥ ���� ��ư Ȱ��ȭ
				SetWindowTextW(hwndKeyCodeInput[index], L""); // Ű �̸� �ʱ�ȭ
				realKeyCode[index] = 0; // Ű �ڵ� �ʱ�ȭ
			}
		}
		else if (LOWORD(wp) >= 2000 && LOWORD(wp) < 3000) { // ���� ��ư ID ����
			int index = LOWORD(wp) - 2000;

			// �ش� �ε����� ��� UI ��� ����
			DestroyWindow(hwndHour[index]);
			DestroyWindow(hwndMinute[index]);
			DestroyWindow(hwndSecond[index]);
			DestroyWindow(hwndMillisecond[index]);
			DestroyWindow(hwndX[index]);
			DestroyWindow(hwndY[index]);
			DestroyWindow(hwndSetPosButton[index]);
			DestroyWindow(hwndKeyPressCheckBox[index]);
			DestroyWindow(hwndKeyCodeInput[index]);
			DestroyWindow(hwndDeleteButton[index]);

			// ���Ϳ��� ��� ����
			hwndHour.erase(hwndHour.begin() + index);
			hwndMinute.erase(hwndMinute.begin() + index);
			hwndSecond.erase(hwndSecond.begin() + index);
			hwndMillisecond.erase(hwndMillisecond.begin() + index);
			hwndX.erase(hwndX.begin() + index);
			hwndY.erase(hwndY.begin() + index);
			hwndSetPosButton.erase(hwndSetPosButton.begin() + index);
			hwndKeyPressCheckBox.erase(hwndKeyPressCheckBox.begin() + index);
			hwndKeyCodeInput.erase(hwndKeyCodeInput.begin() + index);
			hwndDeleteButton.erase(hwndDeleteButton.begin() + index);
			realKeyCode.erase(realKeyCode.begin() + index);

			currentClickCount--;

			//������ ������ �����ư ��Ȱ��ȭ
			if (currentClickCount == 0) {
				EnableWindow(hwndStartMacroButton, FALSE);
				EnableWindow(hwndTimeBulkChangeButton, FALSE);
				EnableWindow(hwndCoordinateBulkChangeButton, FALSE);
			}
			// ������ UI ��� ��ġ ����
			for (int i = index; i < currentClickCount; ++i) {
				int yOffset = 95 + i * 28;

				SetWindowPos(hwndHour[i], NULL, 20, yOffset, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
				SetWindowPos(hwndMinute[i], NULL, 45, yOffset, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
				SetWindowPos(hwndSecond[i], NULL, 70, yOffset, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
				SetWindowPos(hwndMillisecond[i], NULL, 95, yOffset, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
				SetWindowPos(hwndX[i], NULL, 140, yOffset, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
				SetWindowPos(hwndY[i], NULL, 185, yOffset, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
				SetWindowPos(hwndSetPosButton[i], NULL, 230, yOffset, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
				SetWindowPos(hwndKeyPressCheckBox[i], NULL, 325, yOffset, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
				SetWindowPos(hwndKeyCodeInput[i], NULL, 375, yOffset, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
				SetWindowPos(hwndDeleteButton[i], NULL, 415, yOffset, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

				// ID �缳��
				SetWindowLongPtr(hwndSetPosButton[i], GWLP_ID, 6 + i);               // Set Pos ��ư ID
				SetWindowLongPtr(hwndKeyPressCheckBox[i], GWLP_ID, 1000 + i);        // KeyPress CheckBox ID
				SetWindowLongPtr(hwndDeleteButton[i], GWLP_ID, 2000 + i);            // Delete ��ư ID
			}

			// â ũ�� ����
			int newHeight = curHeight + currentClickCount * 28;
			SetWindowPos(hwnd, NULL, 0, 0, curWidth, newHeight, SWP_NOZORDER | SWP_NOMOVE);

			// â ���� ����
			InvalidateRect(hwnd, NULL, TRUE);
			UpdateWindow(hwnd);
			}
		break;
	}
	
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, msg, wp, lp);
	}
	return 0;
}

// 'WinMain' �Լ� ����
int APIENTRY WinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	// ������ Ŭ���� ���
	WNDCLASS wc = {};
	curInstance = hInstance;
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = L"AutoClickerWindow";
	wc.hIcon = LoadIcon(curInstance, MAKEINTRESOURCE(IDI_ICON1));
	RegisterClass(&wc);

	// ������ ���� (���� ���� 160, ũ�� ���� ����)
	HWND hwnd = CreateWindowEx(0, L"AutoClickerWindow", L"Auto Clicker", WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, curWidth, curHeight, NULL, NULL, wc.hInstance, NULL);

	if (hwnd == NULL) {
		return 0;
	}

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	// �޽��� ����
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}
