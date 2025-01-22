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
static HWND hwndTimeInputWindow = NULL;  // 일괄 입력 창 핸들
static HWND hwndInputHour, hwndInputMinute, hwndInputSecond, hwndInputMillisecond; // 입력 필드 핸들


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
	MessageBox(hwnd, message.c_str(), L"입력 오류", MB_OK | MB_ICONERROR);
}

bool ValidateTime(HWND hwnd, int hour, int minute, int second, int millisecond) {
	if (hour < 0 || hour > 23) {
		ShowError(hwnd, L"시간 값은 0에서 23 사이여야 합니다.");
		return false;
	}
	if (minute < 0 || minute > 59) {
		ShowError(hwnd, L"분 값은 0에서 59 사이여야 합니다.");
		return false;
	}
	if (second < 0 || second > 59) {
		ShowError(hwnd, L"초 값은 0에서 59 사이여야 합니다.");
		return false;
	}
	if (millisecond < 0 || millisecond > 999) {
		ShowError(hwnd, L"밀리초 값은 0에서 999 사이여야 합니다.");
		return false;
	}
	return true;
}

bool ValidateCoordinate(HWND hwnd, int x, int y) {
	if (x < 0 || y < 0) {
		ShowError(hwnd, L"좌표 값은 0 이상의 숫자여야 합니다.");
		return false;
	}
	if (x > 9999 || y > 9999) {
		ShowError(hwnd, L"좌표 값은 0~9999 사이로 입력해주세요.");
		return false;
	}
	return true;
}

INT_PTR CALLBACK KeyInputDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static int* outputKeyCode;
	static HWND hwndSetKeyButton;
	hwndSetKeyButton = GetDlgItem(hwndDlg, IDC_KEY_SET); // 좌표 지정 버튼

	switch (message) {
	case WM_INITDIALOG:
		outputKeyCode = reinterpret_cast<int*>(lParam); // 전달받은 키 코드를 저장할 포인터
		EnableWindow(GetDlgItem(hwndDlg, IDOK), FALSE); // 확인버튼 비활성화
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
		else if (LOWORD(wParam) == IDC_KEY_SET) {  // "키 지정" 버튼 클릭 시
			// 버튼 텍스트 변경
			SetWindowText(hwndSetKeyButton, L"입력 대기중");

			// 키 입력을 기다림
			while (true) {
				if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {  // 마우스 왼쪽버튼 감지
					SetWindowText(hwndSetKeyButton, L"키 입력");
					break;
				}
				for (int keyCode = 0x01; keyCode <= 0xFE; ++keyCode) {
					SHORT keyState = GetAsyncKeyState(keyCode);
					if (	keyState & 0x8000) { // 키가 눌려 있는 상태인지 확인
						UINT scanCode = MapVirtualKey(keyCode, MAPVK_VK_TO_VSC);
						GetKeyNameText((scanCode << 16), curKeyName, sizeof(curKeyName) / sizeof(wchar_t));
						SetWindowText(hwndSetKeyButton, curKeyName);
						*outputKeyCode = static_cast<int>(wParam);
						curKeyCode = keyCode;
						EnableWindow(GetDlgItem(hwndDlg, IDOK), TRUE); // 확인버튼 활성화
						return (INT_PTR)FALSE;
					}
				}
			}
		}
		break;

	case WM_CLOSE:
		EndDialog(hwndDlg, IDCANCEL); // 취소 시 닫기
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

		// 시스템 시간 가져와서 입력 필드에 설정
		SYSTEMTIME localTime;
		GetLocalTime(&localTime);
		SetWindowText(hwndHourInput, to_wstring(localTime.wHour).c_str());
		SetWindowText(hwndMinuteInput, to_wstring(localTime.wMinute).c_str());
		SetWindowText(hwndSecondInput, to_wstring(localTime.wSecond).c_str());
		SetWindowText(hwndMillisecondInput, L"0");
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) {  // "변경" 버튼 클릭 시
			wchar_t buffer[100];

			// 각 필드에서 값 가져오기
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
		else if (LOWORD(wParam) == IDCANCEL) {  // "취소" 버튼 클릭 시
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
		hwndXInput = GetDlgItem(hwndDlg, IDC_X);  // X 좌표 입력 필드
		hwndYInput = GetDlgItem(hwndDlg, IDC_Y);  // Y 좌표 입력 필드
		hwndSetCoordinateButton = GetDlgItem(hwndDlg, IDC_COORDINATE_SET); // 좌표 지정 버튼

		// 현재 값 (여기서 bulkX, bulkY는 기본값을 가지고 있다고 가정)
		SetWindowText(hwndXInput, to_wstring(bulkX).c_str());
		SetWindowText(hwndYInput, to_wstring(bulkY).c_str());

		return (INT_PTR)TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) {  // "변경" 버튼 클릭 시
			wchar_t buffer[100];
			// X 좌표 가져오기
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
		else if (LOWORD(wParam) == IDCANCEL) {  // "취소" 버튼 클릭 시
			EndDialog(hwndDlg, IDCANCEL);
			return (INT_PTR)TRUE;
		}
		else if (LOWORD(wParam) == IDC_COORDINATE_SET) {  // "좌표 지정" 버튼 클릭 시

			// 버튼 텍스트를 "Z키 입력"으로 변경
			SetWindowText(hwndSetCoordinateButton, L"Z키 입력");

			// Z 키 입력을 기다림
			while (true) {
				if (GetAsyncKeyState('Z') & 0x8000) {  // Z 키가 눌리면
					POINT p;
					GetCursorPos(&p);

					// 좌표를 입력 필드에 설정
					SetWindowText(hwndXInput, to_wstring(p.x).c_str());
					SetWindowText(hwndYInput, to_wstring(p.y).c_str());

					// 버튼 텍스트를 다시 "좌표 지정"으로 변경
					SetWindowText(hwndSetCoordinateButton, L"좌표 지정");
					break;
				}
				if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {  // 마우스 왼쪽버튼 감지
					SetWindowText(hwndSetCoordinateButton, L"좌표 지정");
					break;
				}
				this_thread::sleep_for(chrono::milliseconds(100));
			}
		}
		break;

	}

	return (INT_PTR)FALSE;
}

// 윈도우 프로시저
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

	switch (msg) {  // 여기서부터 switch 문 시작
	case WM_CREATE: {
		hwndStartMacroButton = CreateWindow(L"BUTTON", L"실행", WS_VISIBLE | WS_CHILD, 20, 20, 60, 30, hwnd, (HMENU)2, NULL, NULL);
		hwndTimeBulkChangeButton = CreateWindow(L"BUTTON", L"시간 일괄 변경", WS_VISIBLE | WS_CHILD, 90, 20, 90, 30, hwnd, (HMENU)4, NULL, NULL);
		hwndCoordinateBulkChangeButton = CreateWindow(L"BUTTON", L"좌표 일괄 변경", WS_VISIBLE | WS_CHILD, 190, 20, 90, 30, hwnd, (HMENU)5, NULL, NULL);
		hwndAddClickButton = CreateWindow(L"BUTTON", L"동작 추가", WS_VISIBLE | WS_CHILD, 305, 20, 60, 30, hwnd, (HMENU)1, NULL, NULL);
		hwndRefreshButton = CreateWindow(L"BUTTON", L"초기화", WS_VISIBLE | WS_CHILD, 375, 20, 60, 30, hwnd, (HMENU)3, NULL, NULL);


		hwndHintHour = CreateWindow(L"STATIC", L"시", WS_VISIBLE | WS_CHILD, 20, 65, 20, 20, hwnd, NULL, NULL, NULL);
		hwndHintMinute = CreateWindow(L"STATIC", L"분", WS_VISIBLE | WS_CHILD, 45, 65, 20, 20, hwnd, NULL, NULL, NULL);
		hwndHintSecond = CreateWindow(L"STATIC", L"초", WS_VISIBLE | WS_CHILD, 70, 65, 20, 20, hwnd, NULL, NULL, NULL);
		hwndHintMilliSecond = CreateWindow(L"STATIC", L"ms", WS_VISIBLE | WS_CHILD | SS_CENTER, 95, 65, 30, 20, hwnd, NULL, NULL, NULL);
		hwndHintXY = CreateWindow(L"STATIC", L"좌표 (X,Y)", WS_VISIBLE | WS_CHILD | SS_CENTER, 140, 65, 170, 20, hwnd, NULL, NULL, NULL);
		hwndHintK = CreateWindow(L"STATIC", L"키 입력", WS_VISIBLE | WS_CHILD | SS_CENTER, 325, 65, 75, 20, hwnd, NULL, NULL, NULL);
		hwndHintK = CreateWindow(L"STATIC", L"×", WS_VISIBLE | WS_CHILD | SS_CENTER, 415, 65, 20, 20, hwnd, NULL, NULL, NULL);
		
		//실행 막기
		EnableWindow(hwndStartMacroButton, FALSE);
		EnableWindow(hwndTimeBulkChangeButton, FALSE);
		EnableWindow(hwndCoordinateBulkChangeButton, FALSE);

		//폰트 설정
		hFont = CreateFont(
			-MulDiv(9, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72), // 포인트 크기를 픽셀로 변환
			0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial"
		);
		SendMessage(hwndAddClickButton, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessage(hwndStartMacroButton, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessage(hwndRefreshButton, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessage(hwndTimeBulkChangeButton, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessage(hwndCoordinateBulkChangeButton, WM_SETFONT, (WPARAM)hFont, TRUE);

		// 창 배경을 흰색으로 설정
		HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255)); // 흰색
		SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBrush);
		break;
	}
	case WM_COMMAND: {
		if (LOWORD(wp) == 1) {  // Add Click 버튼 클릭 시

			int yOffset = 95 + currentClickCount * 28;  // 버튼을 누른 뒤 나타나는 입력란들 위치

			SYSTEMTIME localTime;
			GetLocalTime(&localTime);

			hwndHour.push_back(CreateWindow(L"EDIT", to_wstring(localTime.wHour).c_str(), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 20, yOffset, 20, 20, hwnd, NULL, NULL, NULL));
			hwndMinute.push_back(CreateWindow(L"EDIT", to_wstring(localTime.wMinute).c_str(), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 45, yOffset, 20, 20, hwnd, NULL, NULL, NULL));
			hwndSecond.push_back(CreateWindow(L"EDIT", to_wstring(localTime.wSecond).c_str(), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 70, yOffset, 20, 20, hwnd, NULL, NULL, NULL));
			hwndMillisecond.push_back(CreateWindow(L"EDIT", L"0", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 95, yOffset, 30, 20, hwnd, NULL, NULL, NULL));

			hwndX.push_back(CreateWindow(L"EDIT", L"0", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 140, yOffset, 40, 20, hwnd, NULL, NULL, NULL));  // 기본값 0
			hwndY.push_back(CreateWindow(L"EDIT", L"0", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 185, yOffset, 40, 20, hwnd, NULL, NULL, NULL));  // 기본값 0
			hwndSetPosButton.push_back(CreateWindow(L"BUTTON", L"좌표 지정", WS_VISIBLE | WS_CHILD, 230, yOffset, 80, 20, hwnd, reinterpret_cast<HMENU>(static_cast<ULONG_PTR>(6 + currentClickCount)), NULL, NULL));

			hwndKeyPressCheckBox.push_back(CreateWindow(L"BUTTON", L"off", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 325, yOffset, 40, 20, hwnd, reinterpret_cast<HMENU>(1000 + currentClickCount), NULL, NULL));
			hwndKeyCodeInput.push_back(CreateWindow(L"STATIC", L"", WS_VISIBLE | WS_CHILD, 370, yOffset, 30, 20, hwnd, NULL, NULL, NULL));
			hwndDeleteButton.push_back(CreateWindow(L"BUTTON", L"×", WS_VISIBLE | WS_CHILD, 415, yOffset, 20, 20, hwnd, reinterpret_cast<HMENU>(2000 + currentClickCount), NULL, NULL));

			realKeyCode.push_back(0);
			currentClickCount++;

			// 창 크기 동적으로 변경 (최소 높이 160, 필요한 만큼 늘려줌)
			int newHeight = curHeight + currentClickCount * 28;
			SetWindowPos(hwnd, NULL, 0, 0, curWidth, newHeight, SWP_NOZORDER | SWP_NOMOVE);
			
			//버튼 활성화
			EnableWindow(hwndStartMacroButton, TRUE);
			EnableWindow(hwndTimeBulkChangeButton, TRUE);
			EnableWindow(hwndCoordinateBulkChangeButton, TRUE);
			
			// 창 내용 갱신
			InvalidateRect(hwnd, NULL, TRUE);
			UpdateWindow(hwnd);
		}
		else if (LOWORD(wp) == 2) {  // Start Macro 버튼 클릭 시
			clicks.clear();

			// 입력된 값들을 클릭 목록에 추가
			for (int i = 0; i < currentClickCount; ++i) {
				ClickInput click;
				wchar_t buffer[100];

				GetWindowText(hwndX[i], buffer, sizeof(buffer) / sizeof(wchar_t));
				click.x = wcstol(buffer, NULL, 10);
				GetWindowText(hwndY[i], buffer, sizeof(buffer) / sizeof(wchar_t));
				click.y = wcstol(buffer, NULL, 10);

				if (!ValidateCoordinate(hwnd, click.x, click.y)) {
					return 0;  // 에러 메시지 출력 후 실행 중단
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
					return 0;  // 에러 메시지 출력 후 실행 중단
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

			// 매크로 실행
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
					// 키 입력 처리
					keybd_event(click.keyCode, 0, 0, 0);  // 키 누름
					keybd_event(click.keyCode, 0, KEYEVENTF_KEYUP, 0); // 키 뗌
				}
				else {
					SetCursorPos(click.x, click.y);
					mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
					mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
				}
			}
		}
		else if (LOWORD(wp) == 3) {  // Refresh 버튼 클릭 시
			clicks.clear();
			currentClickCount = 0;

			// 모든 입력 필드를 제거
			for (int i = 0; i < currentClickCount; ++i) {
				DestroyWindow(hwndX[i]);
				DestroyWindow(hwndY[i]);
				DestroyWindow(hwndHour[i]);
				DestroyWindow(hwndMinute[i]);
				DestroyWindow(hwndSecond[i]);
				DestroyWindow(hwndMillisecond[i]);
				DestroyWindow(hwndSetPosButton[i]);
			}

			// 창 크기 최소화 및 프로그램 재실행
			HWND hwndCurrent = hwnd;
			PostMessage(hwndCurrent, WM_CLOSE, 0, 0);

			STARTUPINFO si = {};
			PROCESS_INFORMATION pi = {};
			TCHAR szPath[MAX_PATH];
			GetModuleFileName(NULL, szPath, MAX_PATH);  // 현재 실행 중인 프로그램 경로 가져오기

			if (CreateProcess(szPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
				// 프로세스가 성공적으로 생성되면 핸들 닫기
				CloseHandle(pi.hProcess);  // 프로세스 핸들 닫기
				CloseHandle(pi.hThread);   // 스레드 핸들 닫기
			}
			else {
				// CreateProcess 실패 처리 (필요시)
				MessageBox(hwnd, L"프로세스 생성 실패", L"오류", MB_OK | MB_ICONERROR);
			}
		}

		// "시간 일괄변경" 버튼 클릭 시
		else if (LOWORD(wp) == 4) {
			// 다이얼로그 띄우기
			DialogBox(curInstance, MAKEINTRESOURCE(IDD_TIME_BULK_CHANGE), hwnd, TimeBulkChangeDlgProc);

			// 다이얼로그에서 시간이 변경되었으면 메인 윈도우에서 변경된 시간을 사용할 수 있음
			// 예를 들어, 변경된 시간을 표시하는 부분을 추가할 수 있습니다.
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

		// '좌표 일괄변경' 버튼 클릭 시
		else if (LOWORD(wp) == 5) {
			// 다이얼로그 띄우기
			DialogBox(curInstance, MAKEINTRESOURCE(IDD_COORDINATE_BULK_CHANGE), hwnd, CoordinateBulkChangeDlgProc);

			// 다이얼로그에서 좌표가 변경되었으면 메인 윈도우에서 변경된 좌표를 사용할 수 있음
			// 예를 들어, 변경된 좌표를 모든 입력 필드에 설정하는 부분을 추가합니다.

			if (coordinateBulkChange) {
				for (int i = 0; i < currentClickCount; ++i) {
					// 변경된 좌표를 각 입력 필드에 설정
					SetWindowText(hwndX[i], to_wstring(bulkX).c_str());
					SetWindowText(hwndY[i], to_wstring(bulkY).c_str());
				}
				coordinateBulkChange = false;
			}
		}

		// 각 클릭 동작의 Set Pos 버튼 클릭 시, Z 키 입력을 기다리기 위한 이벤트 처리
		else if (LOWORD(wp) >= 6 && LOWORD(wp) < 1000) {
			int index = LOWORD(wp) - 6;

			// 버튼 텍스트를 "Z키 입력"으로 변경
			SetWindowText(hwndSetPosButton[index], L"Z키 입력");

			// Z 키 입력을 기다림
			while (true) {
				if (GetAsyncKeyState('Z') & 0x8000) {  // Z 키가 눌리면
					POINT p;
					GetCursorPos(&p);

					// 좌표를 입력 필드에 설정
					SetWindowText(hwndX[index], to_wstring(p.x).c_str());
					SetWindowText(hwndY[index], to_wstring(p.y).c_str());

					// 버튼 텍스트를 다시 "좌표 지정"으로 변경
					SetWindowText(hwndSetPosButton[index], L"좌표 지정");
					break;
				}
				if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {  // 마우스 왼쪽버튼 감지
					SetWindowText(hwndSetPosButton[index], L"좌표 지정");
					break;
				}
			}
		}

		else if (LOWORD(wp) >= 1000 && LOWORD(wp) < 2000) { // 키 입력 체크박스 ID 범위
			int index = LOWORD(wp) - 1000; // 체크박스에 대한 인덱스
			UINT keyCode = 0;
			if (SendMessage(hwndKeyPressCheckBox[index], BM_GETCHECK, 0, 0) == BST_CHECKED) {

				// 키 입력 다이얼로그 호출
				if (DialogBoxParam(curInstance, MAKEINTRESOURCE(IDD_KEY_INPUT), hwnd, KeyInputDlgProc, reinterpret_cast<LPARAM>(&keyCode)) == IDOK) {
					
					//입력된 키 이름 표시
					SetWindowText(hwndKeyPressCheckBox[index], L"on");
					SetWindowTextW(hwndKeyCodeInput[index], curKeyName);
					realKeyCode[index] = curKeyCode;

					// 좌표 설정 버튼 및 필드 비활성화
					EnableWindow(hwndX[index], FALSE); // X 좌표 필드 비활성화
					EnableWindow(hwndY[index], FALSE); // Y 좌표 필드 비활성화
					EnableWindow(hwndSetPosButton[index], FALSE); // 좌표 지정 버튼 비활성화
				}
				else {
					// 체크박스 선택 취소
					SendMessage(hwndKeyPressCheckBox[index], BM_SETCHECK, BST_UNCHECKED, 0);
				}
			}
			else {
				// 체크박스가 해제되면 좌표 설정을 다시 활성화
				SetWindowText(hwndKeyPressCheckBox[index], L"off");
				EnableWindow(hwndX[index], TRUE); // X 좌표 필드 활성화
				EnableWindow(hwndY[index], TRUE); // Y 좌표 필드 활성화
				EnableWindow(hwndSetPosButton[index], TRUE); // 좌표 지정 버튼 활성화
				SetWindowTextW(hwndKeyCodeInput[index], L""); // 키 이름 초기화
				realKeyCode[index] = 0; // 키 코드 초기화
			}
		}
		else if (LOWORD(wp) >= 2000 && LOWORD(wp) < 3000) { // 삭제 버튼 ID 범위
			int index = LOWORD(wp) - 2000;

			// 해당 인덱스의 모든 UI 요소 제거
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

			// 벡터에서 요소 삭제
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

			//동작이 없으면 실행버튼 비활성화
			if (currentClickCount == 0) {
				EnableWindow(hwndStartMacroButton, FALSE);
				EnableWindow(hwndTimeBulkChangeButton, FALSE);
				EnableWindow(hwndCoordinateBulkChangeButton, FALSE);
			}
			// 나머지 UI 요소 위치 갱신
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

				// ID 재설정
				SetWindowLongPtr(hwndSetPosButton[i], GWLP_ID, 6 + i);               // Set Pos 버튼 ID
				SetWindowLongPtr(hwndKeyPressCheckBox[i], GWLP_ID, 1000 + i);        // KeyPress CheckBox ID
				SetWindowLongPtr(hwndDeleteButton[i], GWLP_ID, 2000 + i);            // Delete 버튼 ID
			}

			// 창 크기 조정
			int newHeight = curHeight + currentClickCount * 28;
			SetWindowPos(hwnd, NULL, 0, 0, curWidth, newHeight, SWP_NOZORDER | SWP_NOMOVE);

			// 창 내용 갱신
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

// 'WinMain' 함수 선언
int APIENTRY WinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	// 윈도우 클래스 등록
	WNDCLASS wc = {};
	curInstance = hInstance;
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = L"AutoClickerWindow";
	wc.hIcon = LoadIcon(curInstance, MAKEINTRESOURCE(IDI_ICON1));
	RegisterClass(&wc);

	// 윈도우 생성 (세로 길이 160, 크기 조절 막기)
	HWND hwnd = CreateWindowEx(0, L"AutoClickerWindow", L"Auto Clicker", WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, curWidth, curHeight, NULL, NULL, wc.hInstance, NULL);

	if (hwnd == NULL) {
		return 0;
	}

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	// 메시지 루프
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}
