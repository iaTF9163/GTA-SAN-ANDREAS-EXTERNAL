#include <windows.h>
#include <tlhelp32.h>

/*
YOU CAN STILL BUY THE OLD VERSIONS OF GTA TRILOGY FROM THE OFFICIAL ROCKSTAR GAMES WEBSITE.
*/

/*
WARNING: SOME OFFSETS MAY NOT BE SUITABLE FOR SOME VERSIONS BOUGHT FROM DIFFERENT MARKETPLACES OF THE GAME.
This offsets you see below are for GTA: San Andreas v1.0.1.33_Core distributed from the offical Rockstar Games website.
*/

uintptr_t gtasaBaseAddress = 0x0; // Later initalized.

uintptr_t pCPlayerData = 0x80F0D0;
    uintptr_t dwMoney = 0xB8;
    uintptr_t bInfiniteRun = 0x14C;
    uintptr_t bFastReload = 0x14D;
    uintptr_t bFireProof = 0x14E;
    uintptr_t cMaxHealth = 0x14F;
    uintptr_t cMaxArmour = 0x150;

uintptr_t pCPed = 0x0; // Later initalized.
    uintptr_t fHealth = 0x540;
    uintptr_t fArmor = 0x548;
    uintptr_t cCurrentWeaponSlot = 0x718;
    uintptr_t pPedWhoDamagedYou = 0x764;
    uintptr_t pTransform = 0x14;
    uintptr_t pPhysicalFlags = 0x40;

typedef struct {
    float x, y, z;
} CVector;

HANDLE process;
DWORD PID;
#define exitlog(errMessage, exitCode) {CloseHandle(process); MessageBoxA(NULL, errMessage, "Message", MB_OK); return exitCode;}

DWORD getPID (wchar_t *exeStr)
{
    DWORD PID = 0;

    // We take a snapshot (like a photo) of the system that captures the current processes.
    // Then we loop through each one of them, comparing their names with the process name. (wchar_t *exeStr)

    // In this line, we specify a structure to store individual processes while we loop through them.
    // The 'W' at the end allows the structure the store 'wchar' values. Which then allows us to securely
    // compare process names that contains letters outside of the standard english alphabet.
    PROCESSENTRY32W process;

    // For security and history purposes, we specify it's own size just in case.
    // (In modern programming you will probably not see these types of lines.)
    process.dwSize = sizeof(PROCESSENTRY32W);

    // We take the snapshot. This function normally looks like this;
    // CreateToolhelp32Snapshot(DWORD dwFlags, DWORD th32ProcessID)
    // This flag used here allows us to capture 'processes' currently running in the specified PID. We will
    // dive deeper in the modBaseAddr() function. The other parameter is for specifying a PID.
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == NULL)
        return 0;

    // We start the loop by capturing the first process. If it fails, abort.
    if (Process32FirstW(hSnapshot, &process)) {
        do {
            // wcsicmp() allows us to compare wchar strings while NOT being case sensitive.
            if (wcsicmp(process.szExeFile, exeStr) == 0) {
                PID = process.th32ProcessID;
                break;
            }
        // We continue the loop by capturing the next process.
        } while (Process32NextW(hSnapshot, &process));
    }

    // It's good practice to close handles you created.
    CloseHandle(hSnapshot);

    return PID;
}

uintptr_t modBaseAddr (wchar_t *name, DWORD pid)
{
    uintptr_t baseAddress = 0;

    // In this line, we create a structure to store modules while we loop through them.
    // The 'W' at the end allows the structure the store 'wchar' values. Which then allows us to securely
    // compare module names that contains letters outside of the standard english alphabet.
    MODULEENTRY32W module;

    // For security and history purposes, we specify it's own size just in case.
    // (In modern programming you will probably not see these types of lines.)
    module.dwSize = sizeof(MODULEENTRY32W);

    // We take the snapshot. This function normally looks like this;
    // CreateToolhelp32Snapshot(DWORD dwFlags, DWORD th32ProcessID)
    // This flag used here is to tell the function that we want to capture the modules (dll files, exe files...)
    // that are being used by the specified PID. The extra '32' there is to capture the 32-bit modules as well.
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (hSnapshot == NULL)
        return 0;

    // We start the loop by capturing the first module. If it fails, abort.
    if (Module32FirstW(hSnapshot, &module)) {
        do {
            // wcsicmp() allows us to compare wchar strings while NOT being case sensitive.
            if (wcsicmp(module.szModule, name) == 0) {
                baseAddress = (uintptr_t)module.modBaseAddr;
                break;
            }
        // We continue the loop by capturing the next module.
        } while (Module32NextW(hSnapshot, &module));
    }

    // It's good practice to close handles you created.
    CloseHandle(hSnapshot);

    return baseAddress;
}

// I don't see any need to explain these Read's & Write's since they are just wrappers around windows's
// own header file. And it's quite possible to see them in any other project that works on other processes.
template <typename type>
type Read (uintptr_t address, int *fail = NULL, HANDLE p = process)
{
    type buffer;
    size_t nBytes = 0;
    ReadProcessMemory(p, (LPCVOID)address, &buffer, sizeof(type), &nBytes);
    if (nBytes != sizeof(type) && fail != NULL)
        *fail = 1;
    return buffer;
}

template <typename type>
WINBOOL Write (uintptr_t address, type value, HANDLE p = process)
{
    size_t nBytes = 0;
    WriteProcessMemory(p, (LPVOID)address, &value, sizeof(type), &nBytes);
    if (nBytes != sizeof(type))
        return FALSE;
    return TRUE;
}

// This function allows us to easily handle check boxes. If they are open, write this value, if they are closed,
// write this value...
template <typename type>
void HandleButton (uintptr_t offset, int id, type valueOn, type valueOff, HWND hwnd, int useValueOff = 1, DWORD minWaitAmount = 0)
{
    // We are creating a static variable so that it doesn't get reset every time we call this function.
    static DWORD lastTick = 0;

    // If the specified button is checked, and enough time has passed, write.
    if (IsDlgButtonChecked(hwnd, id) && GetTickCount() - lastTick >= minWaitAmount)
        Write<type>(offset, valueOn);
    // If the button is not checked, read. If the value is equal to the value we want to set when the button is
    // CHECKED, write the value we want to set when the button is NOT CHECKED. Time regulations is not
    // effective here.
    else if (Read<type>(offset) == valueOn && useValueOff)
        Write<type>(offset, valueOff);

    lastTick = GetTickCount();
}

// This function is used to check if a wchar string consists of just a defined set of wchar's.
int wcsConsistsOf (wchar_t *str, wchar_t *needles)
{
    // We are looping through the str, then looping through the needles.
    // If the wchar we are currently working on equals at least one of the 'needles',
    // allow it. If not, wait for the last needle. And if the last needle also fails, the wchar we are
    // currently working on is not present in our defined set of wchar's. Abort.

    // wcslen is strlen for wchar strings.

    for (int i = 0; i < wcslen(str); i++) {
        for (int j = 0; j < wcslen(needles); j++) {
            if (str[i] == needles[j])
                break;
            else if (j == wcslen(needles) - 1)
                return 0;
        }
    }
    return 1;
}

int TextBoxInput (HWND hwnd, DWORD *dwordVal = NULL, float *floatVal = NULL)
{
    // We request the text that is written in the text box.
    // The 'W' at the end specifies that we also want to capture the characters outside of the standard
    // english alphabet.
    wchar_t content[1024], *end;
    if (GetWindowTextW(hwnd, content, 1024) == 0)
        return 0;

    // If the text doesn't consists of numbers, a dot for floating numbers, and a dash for negative numbers, abort.
    if (!wcsConsistsOf(content, L"0123456789.-")) {
        MessageBoxA(hwnd, "Please enter a valid input.", "Warning", MB_OK);
        return 0;
    }

    if (dwordVal != NULL)
        // wcstoul is used to convert wchar strings to unsigned long values. (DWORD is unsigned long in the windows API)
        // The 10 at the end specifies that we want to handle the string as a base 10. Which, you probably heard before,
        // is because us humans count with a 10 base. 2 is for binary, 16 is for hex...
        *dwordVal = wcstoul(content, &end, 10);
    if (floatVal != NULL)
        // wcstof is used to convert wchar strings to float values. And it handles dots as well. You can set the used
        // locale by calling wcstof_l instead of the raw wcstof which uses the current locale. This allows you to set
        // different locales. In some countries you might see ',' being used to show floating numbers.
        *floatVal = wcstof(content, &end);

    return 1;
}

// This is the standard declaration for window procs. When we create a window, windows handles it in this function.
// HWND is the handle of the window, UINT msg is the current action, and WPARAM & LPARAM's are used for passing in parameters.
LRESULT CALLBACK WindowProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // We create static HWND's for our text boxes because we don't want them to reset everytime this function is called.
    // And since this function is a window proc, it is probably going to be called many times.
    static HWND moneyTB = NULL;
    static HWND posxTB, posyTB, poszTB;

    // Using switch statements to handle window messages are so efficient.
    switch (msg) {
        // If we get a close message, post a quit message with the exit code 0. This makes the GetMessage() return 0 which
        // then breaks out of the while loop.
        case WM_CLOSE:
            PostQuitMessage(0);
            break;
        // When we are creating the window, create our buttons.
        case WM_CREATE:
            CreateWindowA("BUTTON", "Infinite Run", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 20,  20, 100, 25, hwnd, (HMENU)1, NULL, NULL);
            CreateWindowA("BUTTON", "Fast Reload ", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 20,  50, 100, 25, hwnd, (HMENU)2, NULL, NULL);
            CreateWindowA("BUTTON", "Fire Proof  ", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 20,  80, 100, 25, hwnd, (HMENU)3, NULL, NULL);
            CreateWindowA("BUTTON", "Heal CJ     ", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON  , 20, 110, 100, 25, hwnd, (HMENU)4, NULL, NULL);
            CreateWindowA("BUTTON", "End It All  ", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON  , 20, 140, 100, 25, hwnd, (HMENU)5, NULL, NULL);
            CreateWindowA("BUTTON", "Max Armor   ", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON  , 20, 170, 100, 25, hwnd, (HMENU)6, NULL, NULL);
            CreateWindowA("BUTTON", "Deposit     ", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON  , 20, 200, 100, 25, hwnd, (HMENU)7, NULL, NULL);
            CreateWindowA("BUTTON", "Teleport    ", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON  , 265, 170, 100, 25, hwnd, (HMENU)9, NULL, NULL);
            moneyTB = CreateWindowA("EDIT", "1000", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, 130, 200, 100, 25, hwnd, NULL, NULL, NULL);
            posxTB = CreateWindowA("EDIT", "0", WS_VISIBLE | WS_CHILD | WS_BORDER, 265, 140, 100, 25, hwnd, NULL, NULL, NULL);
            posyTB = CreateWindowA("EDIT", "0", WS_VISIBLE | WS_CHILD | WS_BORDER, 265, 110, 100, 25, hwnd, NULL, NULL, NULL);
            poszTB = CreateWindowA("EDIT", "0", WS_VISIBLE | WS_CHILD | WS_BORDER, 265, 80, 100, 25, hwnd, NULL, NULL, NULL);
            break;
        // When a command is send,
        case WM_COMMAND:
            switch (wParam) {
                // ID 4 -> Heal CJ.
                case 4:
                    Write<float>(pCPed + fHealth, 100.0f);
                    break;
                // ID 5 -> End It All. Well, I respect your decision.
                case 5:
                    Write<float>(pCPed + fHealth, 0.0f);
                    break;
                // ID 6 -> Max Armor.
                case 6:
                    Write<float>(pCPed + fArmor, 100.0f);
                    break;
                // ID 7 -> Deposit.
                case 7:
                    {
                        DWORD value;
                        if (TextBoxInput(moneyTB, &value))
                            Write<DWORD>(pCPlayerData + dwMoney, value);
                        break;
                    }
                // ID 9 -> Teleport.
                case 9:
                    {
                        // We multiply the return values 3 times because even if one of them returns 0 the success variable will be 0.
                        float x, y, z;
                        int success = 1;
                        success *= TextBoxInput(posxTB, NULL, &x);
                        success *= TextBoxInput(posyTB, NULL, &y);
                        success *= TextBoxInput(poszTB, NULL, &z);
                        if (success != 0)
                            Write<CVector>(Read<DWORD>(pCPed + pTransform) + 0x30, {x, y, z});
                        break;
                    }
                // Other ID's are checkboxes, and they are handled with the HandleButton() function in the main loop.
            }
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// This is a wrapper for the windows.h CreateWindow. Just for the sake of readability in the main loop.
HWND createWindow (HINSTANCE hInstance)
{
    // WNDCLASS is used for defining a window class. These classes are used by their name. (lpszClassName)
    WNDCLASSA wc = { 0 };
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hInstance = hInstance;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = "WindowClass";

    // Register the currently created window class to the ecosystem.
    RegisterClassA(&wc);

    // Create the window. I of course won't be explaining the whole CreateWindowA function here but the most important part
    // here is the WS_OVERLAPPEDWINDOW flag that specifies that other windows will be inside of this window. (Our buttons.)
    HWND hwnd = CreateWindowA("WindowClass", "Grand Theft Auto: San Andreas | EXTERNAL", WS_OVERLAPPEDWINDOW, 0, 0, 400, 300, NULL, NULL, hInstance, NULL);
    if (hwnd == NULL)
        return NULL;

    ShowWindow(hwnd, SW_SHOWNORMAL);

    return hwnd;
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // Get the PID of gta san andreas.
    PID = getPID(L"gta_sa.exe");
    if (PID == 0)
        exitlog("Cannot find gta_sa.exe. Make sure the game is running.", 1);

    // Attach to it.
    process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, PID);
    if (process == NULL)
        exitlog("Cannot attach to gta_sa.exe. Make sure you have the proper rights to do so.", 1);

    // Get the base address and make some last touches.
    gtasaBaseAddress = modBaseAddr(L"gta_sa.exe", PID);
    pCPlayerData += gtasaBaseAddress;
    pCPed = Read<DWORD>(pCPlayerData);

    // Create the window.
    HWND hWindow = NULL;
    hWindow = createWindow(hInstance);
    if (hWindow == NULL)
        exitlog("Cannot create window.", 1);

    // Main Loop.
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        HandleButton<int>(pCPlayerData + bInfiniteRun, 1, 1, 0, hWindow);
        HandleButton<int>(pCPlayerData + bFastReload , 2, 1, 0, hWindow);
        HandleButton<int>(pCPlayerData + bFireProof  , 3, 1, 0, hWindow);
    }

    CloseHandle(process);

    return 0;
}
