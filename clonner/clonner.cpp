// clonner.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "framework.h"
#include "clonner.h"
#include <shlwapi.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <random>
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Shell32.lib")

#define MAX_LOADSTRING 100

// 子コントロール ID（コードからのみ参照）
#define IDC_EDIT_WATCH     2001
#define IDC_BTN_BROWSE_W   2002
#define IDC_EDIT_DEST      2003
#define IDC_BTN_BROWSE_D   2004
#define IDC_EDIT_EXT       2005
#define IDC_BTN_START      2006
#define IDC_BTN_STOP       2007
#define IDC_LIST_LOG       2008
#define IDC_EDIT_INTERVAL  2009

// ログ通知用カスタムメッセージ（WPARAM = new std::wstring*）
#define WM_APP_LOG         (WM_APP + 1)

// グローバル変数:
HINSTANCE hInst;                                // 現在のインターフェイス
WCHAR szTitle[MAX_LOADSTRING];                  // タイトル バーのテキスト
WCHAR szWindowClass[MAX_LOADSTRING];            // メイン ウィンドウ クラス名

// 監視設定（UI から書き換えられる）
static std::wstring g_watchFolder = L"C:\\WatchSource";
static std::wstring g_destFolder  = L"C:\\WatchDest";
static std::wstring g_targetExt   = L"*.txt"; // 複数指定可（カンマ / セミコロン / 空白区切り、例: "*.pdf,*.xlsx"）
static std::vector<std::wstring> g_targetExtList; // 正規化済み拡張子一覧（「.pdf」の形式、小文字とは限らない）
// 監視間隔はランダム化（毎回 [g_intervalMinMs, g_intervalMaxMs] の範囲）
static DWORD        g_intervalMinMs = 1000; // 下限 1 秒（固定）
static DWORD        g_intervalMaxMs = 2000; // 上限（UI から書き換えられる）

// フォルダ監視用
static HANDLE g_hWatchThread = nullptr;
static HANDLE g_hStopEvent   = nullptr;
static HWND   g_hMainWnd     = nullptr;
static HWND   g_hLog         = nullptr;

// このコード モジュールに含まれる関数の宣言を転送します:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
static DWORD WINAPI WatchThreadProc(LPVOID lpParam);
static bool         StartWatching();
static void         StopWatching();
static void         CreateChildControls(HWND hWnd);
static void         LayoutChildControls(HWND hWnd);
static bool         BrowseForFolder(HWND hOwner, std::wstring& outPath);
static void         AppendLog(const std::wstring& msg);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: ここにコードを挿入してください。

    // グローバル文字列を初期化する
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CLONNER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // アプリケーション初期化の実行:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CLONNER));

    MSG msg;

    // メイン メッセージ ループ:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  関数: MyRegisterClass()
//
//  目的: ウィンドウ クラスを登録します。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CLONNER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_CLONNER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   関数: InitInstance(HINSTANCE, int)
//
//   目的: インスタンス ハンドルを保存して、メイン ウィンドウを作成します
//
//   コメント:
//
//        この関数で、グローバル変数でインスタンス ハンドルを保存し、
//        メイン プログラム ウィンドウを作成および表示します。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // グローバル変数にインスタンス ハンドルを格納する

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0,640, 420, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  関数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目的: メイン ウィンドウのメッセージを処理します。
//
//  WM_COMMAND  - アプリケーション メニューの処理
//  WM_PAINT    - メイン ウィンドウを描画する
//  WM_DESTROY  - 中止メッセージを表示して戻る
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        g_hMainWnd = hWnd;
        CreateChildControls(hWnd);
        break;
    case WM_SIZE:
        LayoutChildControls(hWnd);
        break;
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 選択されたメニューの解析:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            case IDC_BTN_BROWSE_W:
                {
                    std::wstring p;
                    if (BrowseForFolder(hWnd, p))
                        SetDlgItemTextW(hWnd, IDC_EDIT_WATCH, p.c_str());
                }
                break;
            case IDC_BTN_BROWSE_D:
                {
                    std::wstring p;
                    if (BrowseForFolder(hWnd, p))
                        SetDlgItemTextW(hWnd, IDC_EDIT_DEST, p.c_str());
                }
                break;
            case IDC_BTN_START:
                {
                    WCHAR buf[MAX_PATH] = {};
                    GetDlgItemTextW(hWnd, IDC_EDIT_WATCH, buf, MAX_PATH);
                    g_watchFolder = buf;
                    GetDlgItemTextW(hWnd, IDC_EDIT_DEST, buf, MAX_PATH);
                    g_destFolder = buf;
                    WCHAR ext[256] = {};
                    GetDlgItemTextW(hWnd, IDC_EDIT_EXT, ext, 256);
                    g_targetExt = ext;
                    // "*.pdf,*.xlsx" / ".pdf;xlsx" / "pdf xlsx" などをパースして「.xxx」形式の一覧にする
                    g_targetExtList.clear();
                    {
                        std::wstring token;
                        auto flush = [&]() {
                            // 前後の空白と '*' を除去
                            size_t b = 0, e = token.size();
                            while (b < e && (token[b] == L' ' || token[b] == L'\t' || token[b] == L'*')) ++b;
                            while (e > b && (token[e-1] == L' ' || token[e-1] == L'\t')) --e;
                            std::wstring t = token.substr(b, e - b);
                            if (!t.empty())
                            {
                                if (t[0] != L'.') t = L"." + t;
                                if (t.size() > 1) g_targetExtList.push_back(t);
                            }
                            token.clear();
                        };
                        for (wchar_t ch : g_targetExt)
                        {
                            if (ch == L',' || ch == L';' || ch == L'|' || ch == L' ' || ch == L'\t')
                                flush();
                            else
                                token.push_back(ch);
                        }
                        flush();
                    }
                    if (g_targetExtList.empty())
                    {
                        // フォールバック：入力が空だったら .txt を使う
                        g_targetExtList.push_back(L".txt");
                    }

                    // 監視間隔（秒・最大値）取得
                    BOOL trans = FALSE;
                    UINT sec = GetDlgItemInt(hWnd, IDC_EDIT_INTERVAL, &trans, FALSE);
                    if (!trans || sec == 0) sec = 2;
                    if (sec > 3600) sec = 3600; // 上限 1 時間
                    g_intervalMaxMs = sec * 1000;
                    if (g_intervalMaxMs < g_intervalMinMs)
                        g_intervalMaxMs = g_intervalMinMs;

                    if (g_watchFolder.empty() || g_destFolder.empty())
                    {
                        MessageBoxW(hWnd, L"Please specify both the watch folder and the destination folder.",
                                    L"clonner", MB_OK | MB_ICONWARNING);
                        break;
                    }
                    if (StartWatching())
                    {
                        EnableWindow(GetDlgItem(hWnd, IDC_BTN_START), FALSE);
                        EnableWindow(GetDlgItem(hWnd, IDC_BTN_STOP),  TRUE);
                        EnableWindow(GetDlgItem(hWnd, IDC_EDIT_WATCH), FALSE);
                        EnableWindow(GetDlgItem(hWnd, IDC_EDIT_DEST),  FALSE);
                        EnableWindow(GetDlgItem(hWnd, IDC_EDIT_EXT),   FALSE);
                        EnableWindow(GetDlgItem(hWnd, IDC_EDIT_INTERVAL), FALSE);
                        EnableWindow(GetDlgItem(hWnd, IDC_BTN_BROWSE_W), FALSE);
                        EnableWindow(GetDlgItem(hWnd, IDC_BTN_BROWSE_D), FALSE);
                        WCHAR info[256];
                        wsprintfW(info, L"Watching started (random interval %u-%u s): ",
                                  (g_intervalMinMs / 1000), (g_intervalMaxMs / 1000));
                        // 拡張子一覧をカンマ区切りで表示
                        std::wstring extJoined;
                        for (size_t i = 0; i < g_targetExtList.size(); ++i)
                        {
                            if (i) extJoined += L", ";
                            extJoined += L"*";
                            extJoined += g_targetExtList[i];
                        }
                        AppendLog(std::wstring(info) + g_watchFolder +
                                  L"  ->  " + g_destFolder +
                                  L"  (" + extJoined + L")");
                    }
                    else
                    {
                        MessageBoxW(hWnd, L"Failed to start watching. Please check the folder paths.",
                                    L"clonner", MB_OK | MB_ICONERROR);
                    }
                }
                break;
            case IDC_BTN_STOP:
                StopWatching();
                EnableWindow(GetDlgItem(hWnd, IDC_BTN_START), TRUE);
                EnableWindow(GetDlgItem(hWnd, IDC_BTN_STOP),  FALSE);
                EnableWindow(GetDlgItem(hWnd, IDC_EDIT_WATCH), TRUE);
                EnableWindow(GetDlgItem(hWnd, IDC_EDIT_DEST),  TRUE);
                EnableWindow(GetDlgItem(hWnd, IDC_EDIT_EXT),   TRUE);
                EnableWindow(GetDlgItem(hWnd, IDC_EDIT_INTERVAL), TRUE);
                EnableWindow(GetDlgItem(hWnd, IDC_BTN_BROWSE_W), TRUE);
                EnableWindow(GetDlgItem(hWnd, IDC_BTN_BROWSE_D), TRUE);
                AppendLog(L"Watching stopped.");
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_APP_LOG:
        {
            std::wstring* p = reinterpret_cast<std::wstring*>(wParam);
            if (p)
            {
                if (g_hLog)
                {
                    int idx = (int)SendMessageW(g_hLog, LB_ADDSTRING, 0, (LPARAM)p->c_str());
                    if (idx != LB_ERR)
                        SendMessageW(g_hLog, LB_SETTOPINDEX, idx, 0);
                }
                delete p;
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: HDC を使用する描画コードをここに追加してください...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        StopWatching();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// バージョン情報ボックスのメッセージ ハンドラーです。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

//
// 監視スレッドの開始 / 停止
//
static bool StartWatching()
{
    // 既に動作中なら何もしない
    if (g_hWatchThread) return true;

    // 監視元・コピー先フォルダが無ければ作成（UNC パスの場合は自動作成しない）
    auto isUncPath = [](const std::wstring& p) {
        return p.size() >= 2 && p[0] == L'\\' && p[1] == L'\\';
    };
    if (!isUncPath(g_watchFolder)) CreateDirectoryW(g_watchFolder.c_str(), nullptr);
    if (!isUncPath(g_destFolder))  CreateDirectoryW(g_destFolder.c_str(),  nullptr);

    // 監視元フォルダの存在確認
    DWORD attr = GetFileAttributesW(g_watchFolder.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
        return false;

    g_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_hStopEvent) return false;

    g_hWatchThread = CreateThread(nullptr, 0, WatchThreadProc, nullptr, 0, nullptr);
    if (!g_hWatchThread)
    {
        CloseHandle(g_hStopEvent);
        g_hStopEvent = nullptr;
        return false;
    }
    return true;
}

static void StopWatching()
{
    if (g_hStopEvent)
    {
        SetEvent(g_hStopEvent);
    }
    bool threadStillRunning = false;
    if (g_hWatchThread)
    {
        DWORD wr = WaitForSingleObject(g_hWatchThread, 5000);
        if (wr == WAIT_OBJECT_0)
        {
            CloseHandle(g_hWatchThread);
            g_hWatchThread = nullptr;
        }
        else
        {
            // 想定外: スレッドが終了しない。ハンドルを閉じない（リーク）でも UAF を避ける。
            threadStillRunning = true;
        }
    }
    if (g_hStopEvent && !threadStillRunning)
    {
        CloseHandle(g_hStopEvent);
        g_hStopEvent = nullptr;
    }
}

//
// 拡張子の一致判定（大文字小文字を区別しない、複数拡張子対応）
//
static bool HasTargetExtension(const std::wstring& fileName)
{
    const wchar_t* ext = ::PathFindExtensionW(fileName.c_str());
    if (!ext || !*ext) return false;
    for (const auto& e : g_targetExtList)
    {
        if (_wcsicmp(ext, e.c_str()) == 0) return true;
    }
    return false;
}

//
// 1ファイルをコピー先へコピー（リトライ付き）
//
static void CopyOneFile(const std::wstring& relativeName)
{
    std::wstring src = g_watchFolder;
    if (!src.empty() && src.back() != L'\\') src += L'\\';
    src += relativeName;

    // コピー先のサブフォルダを作成
    std::wstring dst = g_destFolder;
    if (!dst.empty() && dst.back() != L'\\') dst += L'\\';
    dst += relativeName;

    // 親ディレクトリを必要に応じて作成
    std::wstring parent = dst.substr(0, dst.find_last_of(L"\\/"));
    if (!parent.empty())
    {
        // 再帰的にディレクトリ作成
        for (size_t pos = 0; pos != std::wstring::npos; )
        {
            pos = parent.find(L'\\', pos + 1);
            std::wstring part = parent.substr(0, pos);
            if (part.size() >= 2) CreateDirectoryW(part.c_str(), nullptr);
        }
    }

    // ファイルが書き込み中の場合に備えてリトライ
    for (int i = 0; i < 5; ++i)
    {
        if (CopyFileW(src.c_str(), dst.c_str(), FALSE))
        {
            AppendLog(L"Copied: " + relativeName);
            return;
        }
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
        {
            return; // 既に消えている
        }
        Sleep(500);
    }
    AppendLog(L"Copy failed: " + relativeName);
}

//
// フォルダ監視スレッド（指定間隔でポーリング）
//
struct CIStringLess
{
    bool operator()(const std::wstring& a, const std::wstring& b) const
    {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    }
};

static void EnumerateTargetFiles(const std::wstring& dir,
                                 const std::wstring& relBase,
                                 std::set<std::wstring, CIStringLess>& out)
{
    std::wstring pattern = dir;
    if (!pattern.empty() && pattern.back() != L'\\') pattern += L'\\';
    pattern += L"*";

    WIN32_FIND_DATAW fd = {};
    HANDLE h = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &fd,
                                FindExSearchNameMatch, nullptr, 0);
    if (h == INVALID_HANDLE_VALUE) return;

    do
    {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;

        std::wstring rel = relBase;
        if (!rel.empty()) rel += L'\\';
        rel += fd.cFileName;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            std::wstring sub = dir;
            if (!sub.empty() && sub.back() != L'\\') sub += L'\\';
            sub += fd.cFileName;
            EnumerateTargetFiles(sub, rel, out);
        }
        else
        {
            if (HasTargetExtension(fd.cFileName))
                out.insert(rel);
        }
    } while (FindNextFileW(h, &fd));

    FindClose(h);
}

static DWORD WINAPI WatchThreadProc(LPVOID /*lpParam*/)
{
    std::set<std::wstring, CIStringLess> known;

    // 起動直後はその時点のファイル一覧を「既知」として記録するだけ
    // （以降に「増えた」ファイルだけをコピー対象とする）
    EnumerateTargetFiles(g_watchFolder, L"", known);

    // ランダム間隔用の乱数生成器
    std::mt19937 rng(static_cast<unsigned>(GetTickCount64()) ^ GetCurrentThreadId());

    while (true)
    {
        // [g_intervalMinMs, g_intervalMaxMs] の範囲でランダムに次回間隔を決定
        DWORD lo = g_intervalMinMs;
        DWORD hi = g_intervalMaxMs;
        if (hi < lo) hi = lo;
        std::uniform_int_distribution<DWORD> dist(lo, hi);
        DWORD waitMs = dist(rng);

        // 停止イベントを待ちつつランダム間隔だけスリープ
        DWORD wait = WaitForSingleObject(g_hStopEvent, waitMs);
        if (wait == WAIT_OBJECT_0)
        {
            // 停止要求
            break;
        }
        if (wait != WAIT_TIMEOUT)
        {
            break;
        }

        std::set<std::wstring, CIStringLess> current;
        EnumerateTargetFiles(g_watchFolder, L"", current);

        // 新規ファイル（current にあって known に無いもの）をコピー
        for (const auto& name : current)
        {
            if (known.find(name) == known.end())
            {
                CopyOneFile(name);
            }
        }

        known.swap(current);
    }

    return 0;
}

//
// UI ヘルパー
//
static void CreateChildControls(HWND hWnd)
{
    HINSTANCE hi = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);

    CreateWindowW(L"STATIC", L"Watch folder:", WS_CHILD | WS_VISIBLE,
                  0, 0, 0, 0, hWnd, (HMENU)IDC_STATIC, hi, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_watchFolder.c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT_WATCH, hi, nullptr);
    CreateWindowW(L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                  0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_BROWSE_W, hi, nullptr);

    CreateWindowW(L"STATIC", L"Destination folder:", WS_CHILD | WS_VISIBLE,
                  0, 0, 0, 0, hWnd, (HMENU)IDC_STATIC, hi, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_destFolder.c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT_DEST, hi, nullptr);
    CreateWindowW(L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                  0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_BROWSE_D, hi, nullptr);

    CreateWindowW(L"STATIC", L"Extensions:", WS_CHILD | WS_VISIBLE,
                  0, 0, 0, 0, hWnd, (HMENU)IDC_STATIC, hi, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_targetExt.c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT_EXT, hi, nullptr);

    CreateWindowW(L"STATIC", L"Max interval (sec):", WS_CHILD | WS_VISIBLE,
                  0, 0, 0, 0, hWnd, (HMENU)IDC_STATIC, hi, nullptr);
    {
        WCHAR initInterval[16];
        wsprintfW(initInterval, L"%u", (unsigned)(g_intervalMaxMs / 1000));
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", initInterval,
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER | ES_RIGHT,
                        0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT_INTERVAL, hi, nullptr);
    }

    CreateWindowW(L"BUTTON", L"Start", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                  0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_START, hi, nullptr);
    HWND hStop = CreateWindowW(L"BUTTON", L"Stop", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                  0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_STOP, hi, nullptr);
    EnableWindow(hStop, FALSE);

    g_hLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                             WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                             0, 0, 0, 0, hWnd, (HMENU)IDC_LIST_LOG, hi, nullptr);

    // 共通フォント
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    for (int id : { IDC_EDIT_WATCH, IDC_BTN_BROWSE_W, IDC_EDIT_DEST, IDC_BTN_BROWSE_D,
                    IDC_EDIT_EXT, IDC_EDIT_INTERVAL, IDC_BTN_START, IDC_BTN_STOP, IDC_LIST_LOG })
    {
        SendMessageW(GetDlgItem(hWnd, id), WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    // STATIC のフォントは EnumChildWindows で
    EnumChildWindows(hWnd, [](HWND h, LPARAM lp) -> BOOL {
        WCHAR cls[32]; GetClassNameW(h, cls, 32);
        if (_wcsicmp(cls, L"Static") == 0)
            SendMessageW(h, WM_SETFONT, (WPARAM)lp, TRUE);
        return TRUE;
    }, (LPARAM)hFont);
}

static void LayoutChildControls(HWND hWnd)
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    const int W = rc.right - rc.left;
    const int H = rc.bottom - rc.top;

    const int pad     = 8;
    const int labelW  = 110;
    const int btnW    = 70;
    const int rowH    = 24;
    const int editH   = 22;
    int y = pad;

    auto place = [&](int id, int x, int yy, int w, int h) {
        HWND c = GetDlgItem(hWnd, id);
        if (c) MoveWindow(c, x, yy, w, h, TRUE);
    };

    // ラベル/編集/参照ボタンの行を描くために、STATIC は EnumChildWindows で順番に
    // 取得するのではなく、作成順に依存しないよう Z オーダーで並べる方が複雑になる。
    // 代わりに、STATIC を 3 つ分まとめて配置し直すために GetWindow を使う。
    // 単純化のため、STATIC は左寄せの固定位置に置く。
    HWND hStatic = GetWindow(hWnd, GW_CHILD);
    // 子のうち、最初の STATIC は GW_CHILD が返すウィンドウから順に並ぶが、
    // Z オーダーは「最後に作られたウィンドウが最上位」のため逆順になり得る。
    // ラベルは固定文言を識別子代わりに使う。
    auto placeLabel = [&](const wchar_t* text, int x, int yy, int w, int h) {
        HWND c = GetWindow(hWnd, GW_CHILD);
        while (c)
        {
            WCHAR cls[32]; GetClassNameW(c, cls, 32);
            if (_wcsicmp(cls, L"Static") == 0)
            {
                WCHAR buf[64]; GetWindowTextW(c, buf, 64);
                if (wcscmp(buf, text) == 0)
                {
                    MoveWindow(c, x, yy, w, h, TRUE);
                    return;
                }
            }
            c = GetWindow(c, GW_HWNDNEXT);
        }
    };

    // 監視フォルダ
    placeLabel(L"Watch folder:", pad, y + 3, labelW, rowH);
    place(IDC_EDIT_WATCH, pad + labelW, y, W - pad*3 - labelW - btnW, editH);
    place(IDC_BTN_BROWSE_W, W - pad - btnW, y, btnW, editH);
    y += rowH + pad / 2;

    // コピー先フォルダ
    placeLabel(L"Destination folder:", pad, y + 3, labelW, rowH);
    place(IDC_EDIT_DEST, pad + labelW, y, W - pad*3 - labelW - btnW, editH);
    place(IDC_BTN_BROWSE_D, W - pad - btnW, y, btnW, editH);
    y += rowH + pad / 2;

    // 対象拡張子（複数可）
    placeLabel(L"Extensions:", pad, y + 3, labelW, rowH);
    place(IDC_EDIT_EXT, pad + labelW, y, 220, editH);

    // 監視間隔（秒・最大値） … 同じ行の右側。実際の待機は 1 秒〜この値の乱数。
    {
        const int intLabelW = 120;
        const int intEditW  = 70;
        int x = pad + labelW + 220 + pad * 2;
        placeLabel(L"Max interval (sec):", x, y + 3, intLabelW, rowH);
        place(IDC_EDIT_INTERVAL, x + intLabelW, y, intEditW, editH);
    }
    y += rowH + pad / 2;

    // 開始/停止 ボタン（独立した行に配置）
    place(IDC_BTN_START, W - pad - btnW*2 - pad, y, btnW, editH);
    place(IDC_BTN_STOP,  W - pad - btnW,         y, btnW, editH);
    y += rowH + pad;

    // ログ表示（残り全部）
    int logH = H - y - pad;
    if (logH < 60) logH = 60;
    place(IDC_LIST_LOG, pad, y, W - pad*2, logH);

    (void)hStatic;
}

static int CALLBACK BFFCallback(HWND hwnd, UINT uMsg, LPARAM /*lParam*/, LPARAM lpData)
{
    if (uMsg == BFFM_INITIALIZED && lpData)
    {
        SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, lpData);
    }
    return 0;
}

static bool BrowseForFolder(HWND hOwner, std::wstring& outPath)
{
    WCHAR display[MAX_PATH] = {};
    // 現在の入力値を初期選択に
    std::wstring initial;
    if (hOwner)
    {
        WCHAR buf[MAX_PATH] = {};
        // どちらの編集欄を初期にするかは呼び出し側の文脈に依存するが、
        // ここでは out として渡された値があればそれを使う設計にしておく。
        initial = outPath;
    }

    BROWSEINFOW bi = {};
    bi.hwndOwner      = hOwner;
    bi.pszDisplayName = display;
    bi.lpszTitle      = L"Select a folder";
    bi.ulFlags        = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn           = BFFCallback;
    bi.lParam         = initial.empty() ? 0 : (LPARAM)initial.c_str();

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return false;

    WCHAR path[MAX_PATH] = {};
    bool ok = SHGetPathFromIDListW(pidl, path) != FALSE;
    CoTaskMemFree(pidl);
    if (ok) outPath = path;
    return ok;
}

static void AppendLog(const std::wstring& msg)
{
    if (!g_hMainWnd) return;
    // ワーカースレッドからも呼ばれるため、メッセージで UI スレッドに渡す
    std::wstring* p = new std::wstring(msg);
    if (!PostMessageW(g_hMainWnd, WM_APP_LOG, (WPARAM)p, 0))
    {
        delete p;
    }
}
