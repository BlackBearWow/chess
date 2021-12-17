#include "framework.h"
#include "chess8.h"
#include <iostream>
#include <Windows.h>
#include <WinSock2.h>
#include <mmsystem.h>
#include <Ole2.h>   //이 줄은 gdiplus.h를 include하기 전에 include해야 한다. 아니면 에러투성이이다.
#include <gdiplus.h>
#include <vector>
#include <atlstr.h>
#pragma comment(lib, "gdiplus") //라이브러리 추가를 해주지 않으면 "알수 없는 참조" 에러가 뜹니다. 비주얼 스튜디오를 사용할때는 꼭
//추가해줘야하는 줄.
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ws2_32.lib")

#define MAX_LOADSTRING 100
#define SIZE    100

#define ID_SOLOPLAY         1000
#define ID_MULTIGAME        1001
#define ID_PLAYMULTIGAME    1002
#define ID_SERVERADDRESS    1003

using namespace std;
using namespace Gdiplus;

typedef struct _tagPiece
{
    int what;
    POINT where;
    bool first = true;
    bool pormotion = false;
} CHESSPIECE;

typedef struct _tagTeam
{
    vector<Image*> image;
    vector<CHESSPIECE> piece;
    vector<CHESSPIECE> attackedPiece;
} TEAM;

typedef struct _tagObjectInfo
{
    int what;
    bool first;
} OBJECTINFO;

typedef struct _tagPow
{
    Image* powImage;
    POINT where;
    float time;
} POW;

// 전역 변수:
HINSTANCE hInst;                                // 현재 인스턴스입니다.
WCHAR szTitle[MAX_LOADSTRING];                  // 제목 표시줄 텍스트입니다.
WCHAR szWindowClass[MAX_LOADSTRING];            // 기본 창 클래스 이름입니다.

HWND hSoloPlay, hMultiGame, hPlayMultiGame, hServerAddress;
HINSTANCE g_hInst;

TCHAR servAddrStr[50] = { 0, };

bool g_loop = true;
HWND g_hWnd;
HDC g_hDC;  //화면DC

LARGE_INTEGER g_tSecond;    //화면 프레임계산을 위한 변수
LARGE_INTEGER g_tTime;
LARGE_INTEGER g_tTime2;
float g_fDeltatime;

HDC backMemDC;  //더블버퍼링의 DC
HBITMAP backBitmap = NULL;
HBITMAP hMyBitmap, holdBitmap;

int gameReady = 0;  //0이면 정지상태. 1이면 솔로플레이 2면 멀티플레이

POINT ptMouse;  //마우스 위치 저장
bool click = false;
bool whosTurn = true;   //true면 1팀 차례 false면 2팀 차례.
bool stillContinue = true;//게임이 끝나면 false
TEAM team1, team2;
Image* chessGrille; //배경화면 이미지
POW powinfo;
OBJECTINFO objectInfo[8][8];    //현재맵 상황
int simulationObjectInfo[8][8]; //시뮬레이션 맵 정보
vector<POINT> youCanGo;
HBRUSH  oddBrush, evenBrush, selectBrush, infoBrush;   //배경화면 브러시 정보 저장용
HPEN    oddPen, evenPen, whitePen, blackPen, selectPen;         //배경화면 브러시 정보 저장용
HFONT hFont;    //폰트 설정
POINT   selectObject[2][2];     //선택했었던 정보 저장
int whoIsWin = -1;      //-1이면 승부중. 0이면 1팀이 승리. 1이면 2팀이 승리.
int drawCount = 0;      //50이 되면 비김.
int Kingcheck = 0;      //0이면 체크 x 1이면 1팀체크상태, 2면 2팀체크상태
bool promotion = false; //프로모션 상태라면
POINT promotionPoint;
bool enPassant;
POINT enPassantPoint;
int castling;

int playerNum = 0;  //멀티플레이할때 필요한 변수. 자신이 무슨 플레이어인지 알려줌. 1또는2
SOCKET hSock;
int changePlayer = 0;
int moved = 0;
int movedPlayer = 1;
int recvPlayerNum = 1;

void selectScreen();
void SoloRun();
void accessServer();
void initBrushAndSelectObject();    //홀수칸은 노란색 짝수칸은 초록색 최근칸은 연두색 초기화
void initTeam();                    //1팀과 2팀 이미지 로드후 번호부여
void initGrille();                  //배경이미지 로드
void timeCount();
void updateObjectInfo();            //objectnfo변수에 현재 맵 상황을 업데이트함
void clicked();                     //클릭됬다면 해야될일
void promotionDo(int* what);                 //프로모션 상태라면 실행할것
void objectMove(int* what);         //기물 이동
void objectSelect(int* retWhat);    //기물 선택
bool isFirstTeam(int x, int y);     //해당칸이 1팀인지 확인
bool isFirstTeamSimul(int x, int y);//해당simulationObjectInfo칸이 1팀인지 확인
bool isSecondTeam(int x, int y);    //해당칸이 2팀인지 확인
bool isSecondTeamSimul(int x, int y);//해당simulationObjectInfo칸이 2팀인지 확인
void eraseFirstTeamObject(int x, int y);//해당칸의 1팀 기물을 삭제
void eraseSecondTeamObject(int x, int y);//해당칸의 2팀 기물을 삭제
void attackObject(int x, int y);    //
bool inMap(int x, int y);           //x, y가 화면을 클릭했는지 확인
bool canMove(int x, int y, int what); //what이 x,y로 간다면, 체크 상황이 되는지 확인한다. 체크가 된다면 움직이지 false반환.
void check();                       //체크상태인지 확인.
void checkmate();                   //체크메이트 상황인지 확인. 스테일메이트 상황까지 체크한다.
void QueensideCastling();           //퀸사이드캐슬링
void KingsideCastling();            //킹사이드 캐슬링
void drawBackground();              //배경화면 출력
void drawLog();                     //상대방이 움직였던 것, 내가 클릭한것 출력
void drawObjects(Graphics* g);      //체스 말 출력
void drawCanGo(Graphics* g);        //갈 수 있는곳 원으로 표시
void drawPow(Graphics* g);          //공격한것은 pow그리기
void drawInfo(Graphics* g);         //누구의 차례인지, 없어진 기물 표시
void winOrDraw();                   //이기거나 비겼다면 화면에 표시
void Error(LPCWSTR m);

void multiRun();
void multiClicked();
void sendData();
void recvData();

// 이 코드 모듈에 포함된 함수의 선언을 전달합니다:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: 여기에 코드를 입력합니다.

    // 전역 문자열을 초기화합니다.
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CHESS8, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 애플리케이션 초기화를 수행합니다:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    //윈소켓 초기화
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)Error(L"WSAStartup() error");

    //화면용 DC 생성
    g_hDC = GetDC(g_hWnd);

    //Gdiplus 사용하기 위해 초기화 작업
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    static RECT rt;
    backMemDC = CreateCompatibleDC(g_hDC);
    backBitmap = CreateCompatibleBitmap(g_hDC, SIZE * 10, SIZE * 8);
    holdBitmap = (HBITMAP)SelectObject(backMemDC, backBitmap);
    //더블버퍼링에 필요한 비트맵 생성

    initBrushAndSelectObject();
    initTeam();
    initGrille();
    updateObjectInfo();

    QueryPerformanceFrequency(&g_tSecond);
    QueryPerformanceCounter(&g_tTime);

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CHESS8));

    MSG msg;

    // 기본 메시지 루프입니다:
    while (true)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Sleep(20);
            if (gameReady == 0)
                selectScreen();
            else if (gameReady == 1)
                SoloRun();
            else if (gameReady == 2)
                multiRun();
        }
    }
    ReleaseDC(g_hWnd, g_hDC);
    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CHESS8));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    //wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_CHESS5);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.

    HWND hWnd = CreateWindowW(szWindowClass, szTitle,
        (WS_OVERLAPPED | WS_SYSMENU),
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    SetWindowPos(hWnd, HWND_TOPMOST, 100, 100,
        SIZE * 8, SIZE * 8, SWP_NOMOVE | SWP_NOZORDER);
    RECT clientRect; GetClientRect(hWnd, &clientRect);
    SetWindowPos(hWnd, HWND_TOPMOST, 100, 100,
        SIZE * 10 + (SIZE * 8 - clientRect.right),    //오른쪽 여백에는 차례, 먹은것 출력
        SIZE * 8 + (SIZE * 8 - clientRect.bottom),
        SWP_NOMOVE | SWP_NOZORDER);
    //화면 사이즈 딱 맞게 조정.

    g_hWnd = hWnd;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        hSoloPlay = CreateWindow(TEXT("button"), TEXT("Solo Play"), WS_CHILD | WS_VISIBLE | WS_BORDER, 300, 400, 100, 40, hWnd, (HMENU)ID_SOLOPLAY, g_hInst, NULL);
        hMultiGame = CreateWindow(TEXT("button"), TEXT("Multi Game"), WS_CHILD | WS_VISIBLE | WS_BORDER, 500, 400, 100, 40, hWnd, (HMENU)ID_MULTIGAME, g_hInst, NULL);
    }
    break;
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // 메뉴 선택을 구문 분석합니다:
        switch (wmId)
        {
        case ID_SOLOPLAY:
            DestroyWindow(hSoloPlay);
            DestroyWindow(hMultiGame);
            gameReady = 1;
            break;
        case ID_MULTIGAME:
            DestroyWindow(hSoloPlay);
            DestroyWindow(hMultiGame);
            hServerAddress= CreateWindow(TEXT("edit"), TEXT("127.0.0.1"), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 300, 400, 200, 40, hWnd, (HMENU)ID_SERVERADDRESS, g_hInst, NULL);
            hPlayMultiGame = CreateWindow(TEXT("button"), TEXT("Play Multi Game"), WS_CHILD | WS_VISIBLE | WS_BORDER, 500, 400, 150, 40, hWnd, (HMENU)ID_PLAYMULTIGAME, g_hInst, NULL);
            break;
        case ID_PLAYMULTIGAME:
            GetWindowText(hServerAddress, servAddrStr, 50);
            DestroyWindow(hServerAddress);
            DestroyWindow(hPlayMultiGame);
            accessServer();
            break;
        case IDM_ABOUT:
            //DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // TODO: 여기에 hdc를 사용하는 그리기 코드를 추가합니다...
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_LBUTTONDOWN:
    {
        if (gameReady == 1) //솔로플레이일때.
        {
            GetCursorPos(&ptMouse);
            ScreenToClient(g_hWnd, &ptMouse);
            click = true;
            SoloRun();
        }
        else if (gameReady == 2) //멀티게임일때.
        {
            if ((whosTurn == true) && (playerNum == 1))
            {
                GetCursorPos(&ptMouse);
                ScreenToClient(g_hWnd, &ptMouse);
                click = true;
                multiRun();
            }
            else if ((whosTurn == false) && (playerNum == 2))
            {
                GetCursorPos(&ptMouse);
                ScreenToClient(g_hWnd, &ptMouse);
                click = true;
                multiRun();
            }
        }
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        exit(1);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 정보 대화 상자의 메시지 처리기입니다.
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

void selectScreen()
{

}

void accessServer()
{
    hSock;
    SOCKADDR_IN servAddr;
    hSock = socket(AF_INET, SOCK_STREAM, 0);
    if (hSock == INVALID_SOCKET) Error(L"socket() error");

    char addr[100];
    int len = WideCharToMultiByte(CP_ACP, 0, servAddrStr, -1, NULL, 0, NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, servAddrStr, -1, addr, len, NULL, NULL);

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    //servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servAddr.sin_addr.s_addr = inet_addr(addr);
    servAddr.sin_port = htons(31313);

    if (connect(hSock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR) Error(L"connect() error");
    char message[30];
    int strLen = recv(hSock, message, sizeof(message) - 1, 0);
    if (strLen == -1) Error(L"recv() error");
    if (message[0] == '1')
        playerNum = 1;
    else
        playerNum = 2;

    gameReady = 2;

    DWORD recvTimeout = 5;  // 0.005초.
    setsockopt(hSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvTimeout, sizeof(recvTimeout));

}

void SoloRun()
{
    static Graphics g(backMemDC);
    static TCHAR str[30];

    timeCount();

    if (click && stillContinue)
    {
        click = false;
        clicked();
    }

    drawBackground();   //배경화면 출력
    drawLog();          //상대방이 움직였던 것, 내가 클릭한것 출력
    //Rectangle(backMemDC, 50, 50, 150, 150);
    //g.DrawImage(team1.image[0], 0, 0, SIZE, SIZE);  //오류검출하기위한 임시로 넣은 그리기.
    drawObjects(&g);    //체스 말 출력
    drawCanGo(&g);      //갈 수 있는곳 원으로 표시
    drawPow(&g);
    drawInfo(&g);
    winOrDraw();

    TCHAR info[30];
    wsprintf(info, L"%d %d", enPassant, castling);
    BitBlt(g_hDC, 0, 0, SIZE * 10, SIZE * 8, backMemDC, 0, 0, SRCCOPY);
}

void initBrushAndSelectObject()
{
    oddBrush = CreateSolidBrush(RGB(0, 160, 0));
    oddPen = CreatePen(PS_SOLID, 1, RGB(0, 160, 0));
    evenBrush = CreateSolidBrush(RGB(220, 220, 0));
    evenPen = CreatePen(PS_SOLID, 1, RGB(220, 220, 0));

    infoBrush = CreateSolidBrush(RGB(0, 200, 200));

    selectBrush = CreateSolidBrush(RGB(130, 190, 0));
    selectPen = CreatePen(PS_SOLID, 1, RGB(130, 190, 0));

    whitePen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    blackPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));

    hFont = CreateFont(22, 0, 0, 0, 0, 0, 0, 0, HANGEUL_CHARSET, 0, 0, 0,
        VARIABLE_PITCH | FF_ROMAN, TEXT("HY견고딕"));

    SelectObject(backMemDC, GetStockObject(NULL_PEN));

    POINT p;
    p.x = -1;
    p.y = -1;
    selectObject[0][0] = p;
    selectObject[0][1] = p;
    selectObject[1][0] = p;
    selectObject[1][1] = p;

    powinfo.powImage = Image::FromFile(L"chessimage\\pow2.png");
    powinfo.time = 2.0;
}

void initTeam()
{
    //이미지 로드
    team1.image.push_back(Image::FromFile(L"chessimage\\pawnB.png"));
    team1.image.push_back(Image::FromFile(L"chessimage\\kingB.png"));
    team1.image.push_back(Image::FromFile(L"chessimage\\queenB.png"));
    team1.image.push_back(Image::FromFile(L"chessimage\\rookB.png"));
    team1.image.push_back(Image::FromFile(L"chessimage\\bishopB.png"));
    team1.image.push_back(Image::FromFile(L"chessimage\\knightB.png"));

    team2.image.push_back(Image::FromFile(L"chessimage\\pawnB2.png"));
    team2.image.push_back(Image::FromFile(L"chessimage\\kingB2.png"));
    team2.image.push_back(Image::FromFile(L"chessimage\\queenB2.png"));
    team2.image.push_back(Image::FromFile(L"chessimage\\rookB2.png"));
    team2.image.push_back(Image::FromFile(L"chessimage\\bishopB2.png"));
    team2.image.push_back(Image::FromFile(L"chessimage\\knightB2.png"));

    CHESSPIECE p;
    //1팀 기물 번호와 위치 지정후 vector에 삽입
    //pqwn 1~8 king 10 queen 20 rook 30,31 bishop 40,41 knight 50,51
    //폰은 처음 움직이면 2칸 움직일 수 있으므로 확인할수있는 변수가 필요함.
    p.what = 1; p.where.x = 0; p.where.y = 1; team1.piece.push_back(p);
    p.what = 2; p.where.x = 1; p.where.y = 1; team1.piece.push_back(p);
    p.what = 3; p.where.x = 2; p.where.y = 1; team1.piece.push_back(p);
    p.what = 4; p.where.x = 3; p.where.y = 1; team1.piece.push_back(p);
    p.what = 5; p.where.x = 4; p.where.y = 1; team1.piece.push_back(p);
    p.what = 6; p.where.x = 5; p.where.y = 1; team1.piece.push_back(p);
    p.what = 7; p.where.x = 6; p.where.y = 1; team1.piece.push_back(p);
    p.what = 8; p.where.x = 7; p.where.y = 1; team1.piece.push_back(p);
    p.what = 10; p.where.x = 4; p.where.y = 0; team1.piece.push_back(p);
    p.what = 20; p.where.x = 3; p.where.y = 0; team1.piece.push_back(p);
    p.what = 30; p.where.x = 0; p.where.y = 0; team1.piece.push_back(p);
    p.what = 31; p.where.x = 7; p.where.y = 0; team1.piece.push_back(p);
    p.what = 40; p.where.x = 2; p.where.y = 0; team1.piece.push_back(p);
    p.what = 41; p.where.x = 5; p.where.y = 0; team1.piece.push_back(p);
    p.what = 50; p.where.x = 1; p.where.y = 0; team1.piece.push_back(p);
    p.what = 51; p.where.x = 6; p.where.y = 0; team1.piece.push_back(p);

    //2팀 기물 번호와 위치 지정후 vector에 삽입
    //pqwn 101~108 king 110 queen 120 rook 130,131 bishop 140,141 knight 150,151
    p.what = 101; p.where.x = 0; p.where.y = 6; team2.piece.push_back(p);
    p.what = 102; p.where.x = 1; p.where.y = 6; team2.piece.push_back(p);
    p.what = 103; p.where.x = 2; p.where.y = 6; team2.piece.push_back(p);
    p.what = 104; p.where.x = 3; p.where.y = 6; team2.piece.push_back(p);
    p.what = 105; p.where.x = 4; p.where.y = 6; team2.piece.push_back(p);
    p.what = 106; p.where.x = 5; p.where.y = 6; team2.piece.push_back(p);
    p.what = 107; p.where.x = 6; p.where.y = 6; team2.piece.push_back(p);
    p.what = 108; p.where.x = 7; p.where.y = 6; team2.piece.push_back(p);
    p.what = 110; p.where.x = 4; p.where.y = 7; team2.piece.push_back(p);
    p.what = 120; p.where.x = 3; p.where.y = 7; team2.piece.push_back(p);
    p.what = 130; p.where.x = 0; p.where.y = 7; team2.piece.push_back(p);
    p.what = 131; p.where.x = 7; p.where.y = 7; team2.piece.push_back(p);
    p.what = 140; p.where.x = 2; p.where.y = 7; team2.piece.push_back(p);
    p.what = 141; p.where.x = 5; p.where.y = 7; team2.piece.push_back(p);
    p.what = 150; p.where.x = 1; p.where.y = 7; team2.piece.push_back(p);
    p.what = 151; p.where.x = 6; p.where.y = 7; team2.piece.push_back(p);

}

void initGrille()
{
    chessGrille = Image::FromFile(L"chessimage\\chessgrille.png");
}

void timeCount()
{
    QueryPerformanceCounter(&g_tTime2);
    g_fDeltatime = (g_tTime2.QuadPart - g_tTime.QuadPart) / (float)g_tSecond.QuadPart;
    g_tTime = g_tTime2;
}

void updateObjectInfo()
{
    memset(&objectInfo, 0, sizeof(objectInfo));
    vector<CHESSPIECE>::iterator iter;
    for (iter = team1.piece.begin(); iter != team1.piece.end(); iter++)
    {
        objectInfo[iter->where.x][iter->where.y].what = iter->what;
        objectInfo[iter->where.x][iter->where.y].first = iter->first;
    }
    for (iter = team2.piece.begin(); iter != team2.piece.end(); iter++)
    {
        objectInfo[iter->where.x][iter->where.y].what = iter->what;
        objectInfo[iter->where.x][iter->where.y].first = iter->first;
    }
}

void clicked()
{
    int x = ptMouse.x / SIZE;
    int y = ptMouse.y / SIZE;
    if (inMap(x, y) == false)
        return;
    static int what;       //현재 클릭된것이 무엇인지 저장하기 위한 변수
    if (promotion)
        promotionDo(&what);
    //프로모션 우선처리
    else
    {
        if (youCanGo.size() != 0)   //이동할 기물을 클릭했을 상태에서 이동가능함
            objectMove(&what);
        //기물 이동. 갈수있는곳을 클릭한다면 이동. 갈수없는곳을 클릭한다면 선택해제
        if (youCanGo.size() == 0)   //갈수 있는곳이 없다면 기물을 선택해야됨. 선택한 기물의 갈수있는곳을 알려줌.
            objectSelect(&what);
        //기물을 선택하면 갈 수 있는곳을 표시함.
    }
}

void promotionDo(int* what)
{
    if ((promotionPoint.x * SIZE <= ptMouse.x) && (ptMouse.x <= (promotionPoint.x * SIZE + SIZE)) &&
        (promotionPoint.y * SIZE <= ptMouse.y) && (ptMouse.y <= (promotionPoint.y * SIZE + SIZE)))
        //프로모션된 폰의 칸을 선택해야 게임이 진행됨
    {
        vector<CHESSPIECE>::iterator iter;
        int objectCode;     //폰에서 objectCode를 더하면 해당 기물로 바뀜.
        if (ptMouse.x < (promotionPoint.x * SIZE + SIZE / 2))
        {
            if (ptMouse.y < (promotionPoint.y * SIZE + SIZE / 2))
                objectCode = 21; //왼쪽위는queen
            else
                objectCode = 41; //왼쪽아래는bishop
        }
        else
        {
            if (ptMouse.y < (promotionPoint.y * SIZE + SIZE / 2))
                objectCode = 31; //오른쪽위는rook
            else
                objectCode = 51; //오른쪽아래는knight
        }
        if (whosTurn)
        {
            for (iter = team1.piece.begin(); iter != team1.piece.end(); iter++)
            {
                if (iter->pormotion)
                {
                    iter->what += objectCode;
                    *what = iter->what;
                    iter->pormotion = false;
                    promotion = false;
                    whosTurn = false;
                    break;
                }
            }
            changePlayer = 1;
            whosTurn = false;
            movedPlayer = 1;
        }
        else
        {
            for (iter = team2.piece.begin(); iter != team2.piece.end(); iter++)
            {
                if (iter->pormotion)
                {
                    iter->what += objectCode;
                    *what = iter->what;
                    iter->pormotion = false;
                    promotion = false;
                    whosTurn = true;
                    break;
                }
            }
            changePlayer = 1;
            whosTurn = true;
            movedPlayer = 2;
        }
    }
    PlaySound(TEXT("chessSound\\pop1.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    updateObjectInfo();
    check();
    checkmate();
}

void objectMove(int* what)
{
    POINT point;        //최근 움직인 기물을 저장하기 위한 변수
    int x = ptMouse.x / SIZE;
    int y = ptMouse.y / SIZE;
    vector<POINT>::iterator pointIter;
    for (pointIter = youCanGo.begin(); pointIter != youCanGo.end(); pointIter++)
    {
        if ((pointIter->x == x) && (pointIter->y == y))
        {
            vector<CHESSPIECE>::iterator iter;
            drawCount++;
            if (whosTurn == true)
            {
                for (iter = team1.piece.begin(); iter != team1.piece.end(); iter++)
                {
                    if (iter->what == *what)
                    {
                        if (*what < 10)
                        {
                            if ((enPassant) && (enPassantPoint.x == x) && (enPassantPoint.y == (y - 1)))
                            {
                                eraseSecondTeamObject(x, y - 1);
                                //앙파상 공격을 한다면, 상대팀 기물 공격
                            }
                            drawCount = 0;
                            if ((y - iter->where.y) == 2)
                            {
                                enPassant = true;
                                enPassantPoint.x = x;
                                enPassantPoint.y = y;
                                //폰이 처음 움직이는거라면 상대가 앙파상 사용할 수 있는 상태
                            }
                            else
                                enPassant = false;
                            if (y == 7)
                            {
                                promotion = true;
                                promotionPoint.x = x;
                                promotionPoint.y = y;
                                iter->pormotion = true;
                            }
                            //폰이 상대 끝에 도달했다면, 프로모션
                        }
                        else
                            enPassant = false;
                        if (castling && (iter->where.y == y))
                        {
                            if ((iter->where.x - 2) == x)   //퀸 사이드 캐슬링
                            {
                                QueensideCastling();
                            }
                            else if ((iter->where.x + 2) == x)   //킹 사이드 캐슬링
                            {
                                KingsideCastling();
                            }
                        }
                        iter->where.x = x;
                        iter->where.y = y;
                        iter->first = false;
                        point.x = x;
                        point.y = y;
                        selectObject[0][1] = point;
                        point.x = -1;
                        point.y = -1;
                        selectObject[1][0] = point;
                        selectObject[1][1] = point;
                        break;
                    }
                }
                //이동
                if (isSecondTeam(x, y))
                    eraseSecondTeamObject(x, y);
                else
                    PlaySound(TEXT("chessSound\\pop1.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
                //가는 곳에 상대팀 기물이 있다면, 상대팀 기물을 공격
                if (promotion == false) {
                    changePlayer = 1;
                    whosTurn = false;
                    movedPlayer = 1;
                }
                //프로모션이 아니라면 팀2로 차례 변경
                else {
                    changePlayer = 0;
                }
                //프로모션이면 차례 안바꿈
                break;
            }
            //team1
            else
            {
                for (iter = team2.piece.begin(); iter != team2.piece.end(); iter++)
                {
                    if (iter->what == *what)
                    {
                        if (*what < 110)
                        {
                            if ((enPassant) && (enPassantPoint.x == x) && (enPassantPoint.y == (y + 1)))
                            {
                                eraseFirstTeamObject(x, y + 1);
                                //앙파상 공격을 한다면, 상대팀 기물 공격
                            }
                            drawCount = 0;
                            if ((iter->where.y - y) == 2)
                            {
                                enPassant = true;
                                enPassantPoint.x = x;
                                enPassantPoint.y = y;
                                //폰이 처음 움직이는거라면 상대가 앙파상 사용할 수 있는 상태
                            }
                            else
                                enPassant = false;
                            if (y == 0)
                            {
                                promotion = true;
                                promotionPoint.x = x;
                                promotionPoint.y = y;
                                iter->pormotion = true;
                            }
                            //폰이 상대 끝에 도달했다면, 프로모션
                        }
                        else
                            enPassant = false;
                        if (castling && (iter->where.y == y))
                        {
                            if ((iter->where.x - 2) == x)   //퀸 사이드 캐슬링
                            {
                                QueensideCastling();
                            }
                            else if ((iter->where.x + 2) == x)   //킹 사이드 캐슬링
                            {
                                KingsideCastling();
                            }
                        }
                        iter->where.x = x;
                        iter->where.y = y;
                        iter->first = false;
                        point.x = x;
                        point.y = y;
                        selectObject[1][1] = point;
                        point.x = -1;
                        point.y = -1;
                        selectObject[0][0] = point;
                        selectObject[0][1] = point;
                        break;
                    }
                }
                //이동
                if (isFirstTeam(x, y))
                    eraseFirstTeamObject(x, y);
                else
                    PlaySound(TEXT("chessSound\\pop1.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
                //가는 곳에 상대팀 기물이 있다면, 상대팀 기물을 공격
                if (promotion == false) {
                    changePlayer = 1;
                    whosTurn = true;
                    movedPlayer = 2;
                }
                //프로모션이 아니라면 팀1로 차례 변경
                else {
                    changePlayer = 0;
                }
                //프로모션이면 차례 안바꿈
                break;
            }
            //team2
            moved = 1;
            break;
        }
        else {
            moved = 0;
        }
    }
    youCanGo.clear();
    updateObjectInfo();
    check();
    checkmate();
}

void objectSelect(int* retWhat)
{
    POINT point;    //최근 클릭된 기물을 저장하기 위한 변수, 갈수있는곳을 저장하기 위한 변수.
    int x = ptMouse.x / SIZE;
    int y = ptMouse.y / SIZE;
    int what;
    castling = false;
    what = objectInfo[x][y].what;
    youCanGo.clear();
    if (what == 0)
    {
        if (whosTurn == true)
        {
            point.x = -1;
            point.y = -1;
            selectObject[0][0] = point;
        }
        else
        {
            point.x = -1;
            point.y = -1;
            selectObject[1][0] = point;
        }
    }
    //빈 공간을 클릭하면 선택했던 기물배경을 정상적인 색갈로 되돌림.
    else if (what < 100 && (whosTurn == true))
    {
        point.x = x;
        point.y = y;
        selectObject[0][0] = point;
        if (what < 10)//pawn
        {
            if ((!isFirstTeam(x, y + 1)) && (!isSecondTeam(x, y + 1)) && inMap(x, y + 1) && canMove(x, y + 1, what))
            {
                point.x = x;
                point.y = y + 1;
                youCanGo.push_back(point);
                if ((objectInfo[x][y].first) && (!isFirstTeam(x, y + 2)) && (!isSecondTeam(x, y + 2)) && inMap(x, y + 2) && canMove(x, y + 2, what))
                {
                    point.x = x;
                    point.y = y + 2;
                    youCanGo.push_back(point);
                }
            }
            //이동가능경로
            if (isSecondTeam(x - 1, y + 1) && inMap(x - 1, y + 1) && canMove(x - 1, y + 1, what))
            {
                point.x = x - 1;
                point.y = y + 1;
                youCanGo.push_back(point);
            }
            if (isSecondTeam(x + 1, y + 1) && inMap(x + 1, y + 1) && canMove(x + 1, y + 1, what))
            {
                point.x = x + 1;
                point.y = y + 1;
                youCanGo.push_back(point);
            }
            //대각선 공격가능경로
            if (enPassant)
            {
                if (inMap(x - 1, y + 1) && canMove(x - 1, y + 1, what) && ((x - 1) == enPassantPoint.x) && (y == enPassantPoint.y))
                {
                    point.x = x - 1;
                    point.y = y + 1;
                    youCanGo.push_back(point);
                }
                if (inMap(x + 1, y + 1) && canMove(x + 1, y + 1, what) && ((x + 1) == enPassantPoint.x) && (y == enPassantPoint.y))
                {
                    point.x = x + 1;
                    point.y = y + 1;
                    youCanGo.push_back(point);
                }
            }
            //앙파상 공격가능경로
        }
        else if (what == 10) //king
        {
            point.x = x - 1; point.y = y - 1;
            if ((!isFirstTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x; point.y = y - 1;
            if ((!isFirstTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x + 1; point.y = y - 1;
            if ((!isFirstTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x - 1; point.y = y;
            if ((!isFirstTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x + 1; point.y = y;
            if ((!isFirstTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x - 1; point.y = y + 1;
            if ((!isFirstTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x; point.y = y + 1;
            if ((!isFirstTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x + 1; point.y = y + 1;
            if ((!isFirstTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            //일반 움직임
            point.x = x; point.y = y;
            if (canMove(point.x, point.y, what) && (objectInfo[x][y].first == true))    //현재 킹이 체크가 아니고 움직이지 않았다면
            {
                if ((objectInfo[0][0].first == true) && (objectInfo[0][0].what == 30))  //왼쪽 룩이 움직이지 않았을때
                {
                    if ((objectInfo[1][0].what == 0) && (objectInfo[2][0].what == 0) && (objectInfo[3][0].what == 0))   //킹과 캐슬링하는 측의 룩 사이에 다른 기물이 있어서는 안 됨
                    {
                        if (canMove(2, 0, what) && canMove(3, 0, what)) //현재 킹이 통과 하는 칸에 적의 기물이 공격/이동이 가능해서는 안 된다
                        {
                            point.x = x - 2;
                            point.y = y;
                            castling = true;
                            youCanGo.push_back(point);
                        }
                    }
                }
                if ((objectInfo[7][0].first == true) && (objectInfo[7][0].what == 31))  //오른쪽 룩이 움직이지 않았을때
                {
                    if ((objectInfo[6][0].what == 0) && (objectInfo[5][0].what == 0))   //킹과 캐슬링하는 측의 룩 사이에 다른 기물이 있어서는 안 됨
                    {
                        if (canMove(5, 0, what) && canMove(6, 0, what)) //현재 킹이 통과 하는 칸에 적의 기물이 공격/이동이 가능해서는 안 된다
                        {
                            point.x = x + 2;
                            point.y = y;
                            castling = true;
                            youCanGo.push_back(point);
                        }
                    }
                }
            }
            //캐슬링
        }
        else if (what < 30) //queen
        {
            for (point.x = x, point.y = y - 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.y--)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isSecondTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x, point.y = y + 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.y++)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isSecondTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x - 1, point.y = y; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x--)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isSecondTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x + 1, point.y = y; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x++)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isSecondTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x - 1, point.y = y - 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x--, point.y--)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isSecondTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x + 1, point.y = y - 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x++, point.y--)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isSecondTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x - 1, point.y = y + 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x--, point.y++)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isSecondTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x + 1, point.y = y + 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x++, point.y++)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isSecondTeam(point.x, point.y)) break;
                }
            }
        }
        else if (what < 40) //rook
        {
            for (point.x = x, point.y = y - 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.y--)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isSecondTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x, point.y = y + 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.y++)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isSecondTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x - 1, point.y = y; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x--)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isSecondTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x + 1, point.y = y; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x++)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isSecondTeam(point.x, point.y)) break;
                }
            }
        }
        else if (what < 50) //bishop
        {
            for (point.x = x - 1, point.y = y - 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x--, point.y--)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isSecondTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x + 1, point.y = y - 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x++, point.y--)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isSecondTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x - 1, point.y = y + 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x--, point.y++)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isSecondTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x + 1, point.y = y + 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x++, point.y++)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isSecondTeam(point.x, point.y)) break;
                }
            }
        }
        else if (what < 60) //knight
        {
            point.x = x - 1; point.y = y - 2;
            if (!isFirstTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x + 1; point.y = y - 2;
            if (!isFirstTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x - 2; point.y = y - 1;
            if (!isFirstTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x + 2; point.y = y - 1;
            if (!isFirstTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x - 2; point.y = y + 1;
            if (!isFirstTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x + 2; point.y = y + 1;
            if (!isFirstTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x - 1; point.y = y + 2;
            if (!isFirstTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x + 1; point.y = y + 2;
            if (!isFirstTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
        }
    }
    //FirstTeam
    else if (what > 100 && (whosTurn == false))
    {
        point.x = x;
        point.y = y;
        selectObject[1][0] = point;
        if (what < 110)//pawn
        {
            if ((!isSecondTeam(x, y - 1)) && (!isFirstTeam(x, y - 1)) && inMap(x, y - 1) && canMove(x, y - 1, what))
            {
                point.x = x;
                point.y = y - 1;
                youCanGo.push_back(point);
                if ((objectInfo[x][y].first) && (!isSecondTeam(x, y - 2)) && (!isFirstTeam(x, y - 2)) && inMap(x, y - 2) && canMove(x, y - 2, what))
                {
                    point.x = x;
                    point.y = y - 2;
                    youCanGo.push_back(point);
                }
            }
            //이동가능경로
            if (isFirstTeam(x - 1, y - 1) && inMap(x - 1, y - 1) && canMove(x - 1, y - 1, what))
            {
                point.x = x - 1;
                point.y = y - 1;
                youCanGo.push_back(point);
            }
            if (isFirstTeam(x + 1, y - 1) && inMap(x + 1, y - 1) && canMove(x + 1, y - 1, what))
            {
                point.x = x + 1;
                point.y = y - 1;
                youCanGo.push_back(point);
            }
            //대각선 공격가능경로
            if (enPassant)
            {
                if (inMap(x - 1, y - 1) && canMove(x - 1, y - 1, what) && ((x - 1) == enPassantPoint.x) && (y == enPassantPoint.y))
                {
                    point.x = x - 1;
                    point.y = y - 1;
                    youCanGo.push_back(point);
                }
                if (inMap(x + 1, y - 1) && canMove(x + 1, y - 1, what) && ((x + 1) == enPassantPoint.x) && (y == enPassantPoint.y))
                {
                    point.x = x + 1;
                    point.y = y - 1;
                    youCanGo.push_back(point);
                }
            }
            //앙파상 공격가능경로
        }
        else if (what == 110) //king
        {
            point.x = x - 1; point.y = y - 1;
            if ((!isSecondTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x; point.y = y - 1;
            if ((!isSecondTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x + 1; point.y = y - 1;
            if ((!isSecondTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x - 1; point.y = y;
            if ((!isSecondTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x + 1; point.y = y;
            if ((!isSecondTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x - 1; point.y = y + 1;
            if ((!isSecondTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x; point.y = y + 1;
            if ((!isSecondTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x + 1; point.y = y + 1;
            if ((!isSecondTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            //일반 움직임
            point.x = x; point.y = y;
            if (canMove(point.x, point.y, what) && (objectInfo[x][y].first == true))    //현재 킹이 체크가 아니고 움직이지 않았다면
            {
                if ((objectInfo[0][7].first == true) && (objectInfo[0][7].what == 130)) //왼쪽 룩이 움직이지 않았을때
                {
                    if ((objectInfo[1][7].what == 0) && (objectInfo[2][7].what == 0) && (objectInfo[3][7].what == 0))   //킹과 캐슬링하는 측의 룩 사이에 다른 기물이 있어서는 안 됨
                    {
                        if (canMove(2, 7, what) && canMove(3, 7, what)) //현재 킹이 통과 하는 칸에 적의 기물이 공격/이동이 가능해서는 안 된다
                        {
                            point.x = x - 2;
                            point.y = y;
                            castling = true;
                            youCanGo.push_back(point);
                        }
                    }
                }
                if ((objectInfo[7][7].first == true) && (objectInfo[7][7].what == 131)) //오른쪽 룩이 움직이지 않았을때
                {
                    if ((objectInfo[6][7].what == 0) && (objectInfo[5][7].what == 0))   //킹과 캐슬링하는 측의 룩 사이에 다른 기물이 있어서는 안 됨
                    {
                        if (canMove(5, 7, what) && canMove(6, 7, what)) //현재 킹이 통과 하는 칸에 적의 기물이 공격/이동이 가능해서는 안 된다
                        {
                            point.x = x + 2;
                            point.y = y;
                            castling = true;
                            youCanGo.push_back(point);
                        }
                    }
                }
            }
            //캐슬링
        }
        else if (what < 130) //queen
        {
            for (point.x = x, point.y = y - 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.y--)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isFirstTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x, point.y = y + 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.y++)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isFirstTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x - 1, point.y = y; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x--)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isFirstTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x + 1, point.y = y; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x++)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isFirstTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x - 1, point.y = y - 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x--, point.y--)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isFirstTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x + 1, point.y = y - 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x++, point.y--)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isFirstTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x - 1, point.y = y + 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x--, point.y++)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isFirstTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x + 1, point.y = y + 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x++, point.y++)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isFirstTeam(point.x, point.y)) break;
                }
            }
        }
        else if (what < 140) //rook
        {
            for (point.x = x, point.y = y - 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.y--)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isFirstTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x, point.y = y + 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.y++)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isFirstTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x - 1, point.y = y; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x--)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isFirstTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x + 1, point.y = y; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x++)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isFirstTeam(point.x, point.y)) break;
                }
            }
        }
        else if (what < 150) //bishop
        {
            for (point.x = x - 1, point.y = y - 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x--, point.y--)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isFirstTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x + 1, point.y = y - 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x++, point.y--)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isFirstTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x - 1, point.y = y + 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x--, point.y++)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isFirstTeam(point.x, point.y)) break;
                }
            }
            for (point.x = x + 1, point.y = y + 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x++, point.y++)
            {
                if (canMove(point.x, point.y, what))
                {
                    youCanGo.push_back(point);
                    if (isFirstTeam(point.x, point.y)) break;
                }
            }
        }
        else if (what < 160) //knight
        {
            point.x = x - 1; point.y = y - 2;
            if (!isSecondTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x + 1; point.y = y - 2;
            if (!isSecondTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x - 2; point.y = y - 1;
            if (!isSecondTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x + 2; point.y = y - 1;
            if (!isSecondTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x - 2; point.y = y + 1;
            if (!isSecondTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x + 2; point.y = y + 1;
            if (!isSecondTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x - 1; point.y = y + 2;
            if (!isSecondTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
            point.x = x + 1; point.y = y + 2;
            if (!isSecondTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) youCanGo.push_back(point);
        }
    }
    //SecondTeam
    *retWhat = what;
    
}

bool canMove(int x, int y, int what)
{
    vector<CHESSPIECE>::iterator iter;
    for (int i = 0; i < 8; i++)
        for (int k = 0; k < 8; k++)
        {
            simulationObjectInfo[i][k] = objectInfo[i][k].what;
            if (simulationObjectInfo[i][k] == what) simulationObjectInfo[i][k] = 0;
        }
    simulationObjectInfo[x][y] = what;
    //만약 움직였다면의 objectInfo를 simulationObjectInfo에 먼저 구현해본다.
    if (whosTurn)
    {
        int team1king = 10;
        for (iter = team2.piece.begin(); iter != team2.piece.end(); iter++)
        {
            if (simulationObjectInfo[iter->where.x][iter->where.y] != iter->what) continue;
            //다음 차례에 먹힌다면, 그 기물은 고려하지 않습니다. iter로 현재 있는 기물을 탐색하기 때문에 simul에서는 적용되지 않아 추가한 문장.
            if (iter->what < 110)   //pawn
            {
                if ((inMap(iter->where.x - 1, iter->where.y - 1)) && (simulationObjectInfo[iter->where.x - 1][iter->where.y - 1] == team1king))
                    return false;
                if ((inMap(iter->where.x + 1, iter->where.y - 1)) && (simulationObjectInfo[iter->where.x + 1][iter->where.y - 1] == team1king))
                    return false;
            }
            else if (iter->what == 110) //king
            {
                if ((inMap(iter->where.x - 1, iter->where.y - 1)) && (simulationObjectInfo[iter->where.x - 1][iter->where.y - 1] == team1king))
                    return false;
                if ((inMap(iter->where.x, iter->where.y - 1)) && (simulationObjectInfo[iter->where.x][iter->where.y - 1] == team1king))
                    return false;
                if ((inMap(iter->where.x + 1, iter->where.y - 1)) && (simulationObjectInfo[iter->where.x + 1][iter->where.y - 1] == team1king))
                    return false;
                if ((inMap(iter->where.x - 1, iter->where.y)) && (simulationObjectInfo[iter->where.x - 1][iter->where.y] == team1king))
                    return false;
                if ((inMap(iter->where.x + 1, iter->where.y)) && (simulationObjectInfo[iter->where.x + 1][iter->where.y] == team1king))
                    return false;
                if ((inMap(iter->where.x - 1, iter->where.y + 1)) && (simulationObjectInfo[iter->where.x - 1][iter->where.y + 1] == team1king))
                    return false;
                if ((inMap(iter->where.x, iter->where.y + 1)) && (simulationObjectInfo[iter->where.x][iter->where.y + 1] == team1king))
                    return false;
                if ((inMap(iter->where.x + 1, iter->where.y + 1)) && (simulationObjectInfo[iter->where.x + 1][iter->where.y + 1] == team1king))
                    return false;
            }
            else if (iter->what < 130) //queen
            {
                int x, y;
                for (x = iter->where.x, y = iter->where.y - 1; inMap(x, y) && (!isSecondTeam(x, y)); y--)
                {
                    if (isFirstTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team1king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x, y = iter->where.y + 1; inMap(x, y) && (!isSecondTeam(x, y)); y++)
                {
                    if (isFirstTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team1king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x - 1, y = iter->where.y; inMap(x, y) && (!isSecondTeam(x, y)); x--)
                {
                    if (isFirstTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team1king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x + 1, y = iter->where.y; inMap(x, y) && (!isSecondTeam(x, y)); x++)
                {
                    if (isFirstTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team1king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x - 1, y = iter->where.y - 1; inMap(x, y) && (!isSecondTeam(x, y)); x--, y--)
                {
                    if (isFirstTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team1king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x - 1, y = iter->where.y + 1; inMap(x, y) && (!isSecondTeam(x, y)); x--, y++)
                {
                    if (isFirstTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team1king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x + 1, y = iter->where.y - 1; inMap(x, y) && (!isSecondTeam(x, y)); x++, y--)
                {
                    if (isFirstTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team1king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x + 1, y = iter->where.y + 1; inMap(x, y) && (!isSecondTeam(x, y)); x++, y++)
                {
                    if (isFirstTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team1king)
                            return false;
                        break;
                    }
                }
            }
            else if (iter->what < 140) //rook
            {
                int x, y;
                for (x = iter->where.x, y = iter->where.y - 1; inMap(x, y) && (!isSecondTeamSimul(x, y)); y--)
                {
                    if (isFirstTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team1king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x, y = iter->where.y + 1; inMap(x, y) && (!isSecondTeamSimul(x, y)); y++)
                {
                    if (isFirstTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team1king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x - 1, y = iter->where.y; inMap(x, y) && (!isSecondTeamSimul(x, y)); x--)
                {
                    if (isFirstTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team1king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x + 1, y = iter->where.y; inMap(x, y) && (!isSecondTeamSimul(x, y)); x++)
                {
                    if (isFirstTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team1king)
                            return false;
                        break;
                    }
                }
            }
            else if (iter->what < 150) //bishop
            {
                int x, y;
                for (x = iter->where.x - 1, y = iter->where.y - 1; inMap(x, y) && (!isSecondTeamSimul(x, y)); x--, y--)
                {
                    if (isFirstTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team1king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x - 1, y = iter->where.y + 1; inMap(x, y) && (!isSecondTeamSimul(x, y)); x--, y++)
                {
                    if (isFirstTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team1king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x + 1, y = iter->where.y - 1; inMap(x, y) && (!isSecondTeamSimul(x, y)); x++, y--)
                {
                    if (isFirstTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team1king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x + 1, y = iter->where.y + 1; inMap(x, y) && (!isSecondTeamSimul(x, y)); x++, y++)
                {
                    if (isFirstTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team1king)
                            return false;
                        break;
                    }
                }
            }
            else if (iter->what < 160) //knight
            {
                if ((inMap(iter->where.x - 1, iter->where.y - 2)) && (simulationObjectInfo[iter->where.x - 1][iter->where.y - 2] == team1king))
                    return false;
                if ((inMap(iter->where.x + 1, iter->where.y - 2)) && (simulationObjectInfo[iter->where.x + 1][iter->where.y - 2] == team1king))
                    return false;
                if ((inMap(iter->where.x - 2, iter->where.y - 1)) && (simulationObjectInfo[iter->where.x - 2][iter->where.y - 1] == team1king))
                    return false;
                if ((inMap(iter->where.x + 2, iter->where.y - 1)) && (simulationObjectInfo[iter->where.x + 2][iter->where.y - 1] == team1king))
                    return false;
                if ((inMap(iter->where.x - 2, iter->where.y + 1)) && (simulationObjectInfo[iter->where.x - 2][iter->where.y + 1] == team1king))
                    return false;
                if ((inMap(iter->where.x + 2, iter->where.y + 1)) && (simulationObjectInfo[iter->where.x + 2][iter->where.y + 1] == team1king))
                    return false;
                if ((inMap(iter->where.x - 1, iter->where.y + 2)) && (simulationObjectInfo[iter->where.x - 1][iter->where.y + 2] == team1king))
                    return false;
                if ((inMap(iter->where.x + 1, iter->where.y + 2)) && (simulationObjectInfo[iter->where.x + 1][iter->where.y + 2] == team1king))
                    return false;
            }
        }
    }
    //team1 차례
    else
    {
        int team2king = 110;
        for (iter = team1.piece.begin(); iter != team1.piece.end(); iter++)
        {
            if (simulationObjectInfo[iter->where.x][iter->where.y] != iter->what) continue;
            //다음 차례에 먹힌다면, 그 기물은 고려하지 않습니다. iter로 현재 있는 기물을 탐색하기 때문에 simul에서는 적용되지 않아 추가한 문장.
            if (iter->what < 10) //pawn
            {
                if ((inMap(iter->where.x - 1, iter->where.y + 1)) && (simulationObjectInfo[iter->where.x - 1][iter->where.y + 1] == team2king))
                    return false;
                if ((inMap(iter->where.x + 1, iter->where.y + 1)) && (simulationObjectInfo[iter->where.x + 1][iter->where.y + 1] == team2king))
                    return false;
            }
            else if (iter->what == 10) //king
            {
                if ((inMap(iter->where.x - 1, iter->where.y - 1)) && (simulationObjectInfo[iter->where.x - 1][iter->where.y - 1] == team2king))
                    return false;
                if ((inMap(iter->where.x, iter->where.y - 1)) && (simulationObjectInfo[iter->where.x][iter->where.y - 1] == team2king))
                    return false;
                if ((inMap(iter->where.x + 1, iter->where.y - 1)) && (simulationObjectInfo[iter->where.x + 1][iter->where.y - 1] == team2king))
                    return false;
                if ((inMap(iter->where.x - 1, iter->where.y)) && (simulationObjectInfo[iter->where.x - 1][iter->where.y] == team2king))
                    return false;
                if ((inMap(iter->where.x + 1, iter->where.y)) && (simulationObjectInfo[iter->where.x + 1][iter->where.y] == team2king))
                    return false;
                if ((inMap(iter->where.x - 1, iter->where.y + 1)) && (simulationObjectInfo[iter->where.x - 1][iter->where.y + 1] == team2king))
                    return false;
                if ((inMap(iter->where.x, iter->where.y + 1)) && (simulationObjectInfo[iter->where.x][iter->where.y + 1] == team2king))
                    return false;
                if ((inMap(iter->where.x + 1, iter->where.y + 1)) && (simulationObjectInfo[iter->where.x + 1][iter->where.y + 1] == team2king))
                    return false;
            }
            else if (iter->what < 30) //queen
            {
                int x, y;
                for (x = iter->where.x, y = iter->where.y - 1; inMap(x, y) && (!isFirstTeamSimul(x, y)); y--)
                {
                    if (isSecondTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team2king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x, y = iter->where.y + 1; inMap(x, y) && (!isFirstTeamSimul(x, y)); y++)
                {
                    if (isSecondTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team2king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x - 1, y = iter->where.y; inMap(x, y) && (!isFirstTeamSimul(x, y)); x--)
                {
                    if (isSecondTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team2king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x + 1, y = iter->where.y; inMap(x, y) && (!isFirstTeamSimul(x, y)); x++)
                {
                    if (isSecondTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team2king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x - 1, y = iter->where.y - 1; inMap(x, y) && (!isFirstTeamSimul(x, y)); x--, y--)
                {
                    if (isSecondTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team2king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x - 1, y = iter->where.y + 1; inMap(x, y) && (!isFirstTeamSimul(x, y)); x--, y++)
                {
                    if (isSecondTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team2king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x + 1, y = iter->where.y - 1; inMap(x, y) && (!isFirstTeamSimul(x, y)); x++, y--)
                {
                    if (isSecondTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team2king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x + 1, y = iter->where.y + 1; inMap(x, y) && (!isFirstTeamSimul(x, y)); x++, y++)
                {
                    if (isSecondTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team2king)
                            return false;
                        break;
                    }
                }
            }
            else if (iter->what < 40) //rook
            {
                int x, y;
                for (x = iter->where.x, y = iter->where.y - 1; inMap(x, y) && (!isFirstTeamSimul(x, y)); y--)
                {
                    if (isSecondTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team2king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x, y = iter->where.y + 1; inMap(x, y) && (!isFirstTeamSimul(x, y)); y++)
                {
                    if (isSecondTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team2king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x - 1, y = iter->where.y; inMap(x, y) && (!isFirstTeamSimul(x, y)); x--)
                {
                    if (isSecondTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team2king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x + 1, y = iter->where.y; inMap(x, y) && (!isFirstTeamSimul(x, y)); x++)
                {
                    if (isSecondTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team2king)
                            return false;
                        break;
                    }
                }
            }
            else if (iter->what < 50) //bishop
            {
                int x, y;
                for (x = iter->where.x - 1, y = iter->where.y - 1; inMap(x, y) && (!isFirstTeamSimul(x, y)); x--, y--)
                {
                    if (isSecondTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team2king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x - 1, y = iter->where.y + 1; inMap(x, y) && (!isFirstTeamSimul(x, y)); x--, y++)
                {
                    if (isSecondTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team2king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x + 1, y = iter->where.y - 1; inMap(x, y) && (!isFirstTeamSimul(x, y)); x++, y--)
                {
                    if (isSecondTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team2king)
                            return false;
                        break;
                    }
                }
                for (x = iter->where.x + 1, y = iter->where.y + 1; inMap(x, y) && (!isFirstTeamSimul(x, y)); x++, y++)
                {
                    if (isSecondTeamSimul(x, y))
                    {
                        if (simulationObjectInfo[x][y] == team2king)
                            return false;
                        break;
                    }
                }
            }
            else if (iter->what < 60) //knight
            {
                if ((inMap(iter->where.x - 1, iter->where.y - 2)) && (simulationObjectInfo[iter->where.x - 1][iter->where.y - 2] == team2king))
                    return false;
                if ((inMap(iter->where.x + 1, iter->where.y - 2)) && (simulationObjectInfo[iter->where.x + 1][iter->where.y - 2] == team2king))
                    return false;
                if ((inMap(iter->where.x - 2, iter->where.y - 1)) && (simulationObjectInfo[iter->where.x - 2][iter->where.y - 1] == team2king))
                    return false;
                if ((inMap(iter->where.x + 2, iter->where.y - 1)) && (simulationObjectInfo[iter->where.x + 2][iter->where.y - 1] == team2king))
                    return false;
                if ((inMap(iter->where.x - 2, iter->where.y + 1)) && (simulationObjectInfo[iter->where.x - 2][iter->where.y + 1] == team2king))
                    return false;
                if ((inMap(iter->where.x + 2, iter->where.y + 1)) && (simulationObjectInfo[iter->where.x + 2][iter->where.y + 1] == team2king))
                    return false;
                if ((inMap(iter->where.x - 1, iter->where.y + 2)) && (simulationObjectInfo[iter->where.x - 1][iter->where.y + 2] == team2king))
                    return false;
                if ((inMap(iter->where.x + 1, iter->where.y + 2)) && (simulationObjectInfo[iter->where.x + 1][iter->where.y + 2] == team2king))
                    return false;
            }
        }
    }
    //team2 차례
    return true;
}

void eraseFirstTeamObject(int x, int y)
{
    vector<CHESSPIECE>::iterator iter;
    for (iter = team1.piece.begin(); iter != team1.piece.end(); iter++)
    {
        if ((iter->where.x == x) && (iter->where.y == y))
        {
            team1.attackedPiece.push_back(*iter);
            team1.piece.erase(iter);
            drawCount = 0;
            attackObject(x, y);
            break;
        }
    }
}

void eraseSecondTeamObject(int x, int y)
{
    vector<CHESSPIECE>::iterator iter;
    for (iter = team2.piece.begin(); iter != team2.piece.end(); iter++)
    {
        if ((iter->where.x == x) && (iter->where.y == y))
        {
            team2.attackedPiece.push_back(*iter);
            team2.piece.erase(iter);
            drawCount = 0;
            attackObject(x, y);
            break;
        }
    }
}

void attackObject(int x, int y)
{
    powinfo.where.x = x;
    powinfo.where.y = y;
    powinfo.time = 0.0;
    PlaySound(TEXT("chessSound\\pop2.wav"), NULL, SND_NOSTOP | SND_ASYNC);
}

bool isFirstTeam(int x, int y)
{
    if ((0 < objectInfo[x][y].what) && (objectInfo[x][y].what < 100))
        return true;
    else
        return false;
}

bool isFirstTeamSimul(int x, int y)
{
    if ((0 < simulationObjectInfo[x][y]) && (simulationObjectInfo[x][y] < 100))
        return true;
    else
        return false;
}

bool isSecondTeam(int x, int y)
{
    if (100 < objectInfo[x][y].what)
        return true;
    else
        return false;
}

bool isSecondTeamSimul(int x, int y)
{
    if (100 < simulationObjectInfo[x][y])
        return true;
    else
        return false;
}

bool inMap(int x, int y)
{
    if ((x >= 0) && (x < 8) && (y >= 0) && (y < 8))
        return true;
    else
        return false;
}

void check()
{
    int count = 0;  //count가 0이라면 체크
    int what;
    int x, y;
    POINT point;
    vector<CHESSPIECE>::iterator iter;

    Kingcheck = 0;
    if (whosTurn)
    {
        for (iter = team1.piece.begin(); iter != team1.piece.end(); iter++)
        {
            what = iter->what;
            x = iter->where.x;
            y = iter->where.y;
            if (what == 10) //king
            {
                point.x = x; point.y = y;
                if (!canMove(point.x, point.y, what))
                    Kingcheck = 1;
                break;
            }
        }
    }
    else
    {
        for (iter = team2.piece.begin(); iter != team2.piece.end(); iter++)
        {
            what = iter->what;
            x = iter->where.x;
            y = iter->where.y;
            if (what == 110) //king
            {
                point.x = x; point.y = y;
                if (!canMove(point.x, point.y, what))
                    Kingcheck = 2;
                break;
            }
        }
    }
}

void checkmate()
{
    int count = 0;  //count가 0이라면 체크메이트.
    int what;
    int stalemate = 0;  //스테일메이트 상태라면 비긴다.
    int x, y;
    POINT point;
    vector<CHESSPIECE>::iterator iter;
    if (whosTurn)
    {
        for (iter = team1.piece.begin(); iter != team1.piece.end(); iter++)
        {
            what = iter->what;
            x = iter->where.x;
            y = iter->where.y;
            if (what < 10)//pawn
            {
                if ((!isFirstTeam(x, y + 1)) && (!isSecondTeam(x, y + 1)) && inMap(x, y + 1) && canMove(x, y + 1, what))
                {
                    count++;
                    if ((objectInfo[x][y].first) && (!isFirstTeam(x, y + 2)) && (!isSecondTeam(x, y + 2)) && inMap(x, y + 2) && canMove(x, y + 2, what))
                    {
                        count++;
                    }
                }
                //이동가능경로
                if (isSecondTeam(x - 1, y + 1) && inMap(x - 1, y + 1) && canMove(x - 1, y + 1, what))
                {
                    count++;
                }
                if (isSecondTeam(x + 1, y + 1) && inMap(x + 1, y + 1) && canMove(x + 1, y + 1, what))
                {
                    count++;
                }
                //공격가능경로
            }
            else if (what == 10) //king
            {
                point.x = x; point.y = y;   //킹이 자신의 자리에 있을 수 있다면 체크메이트는 아니다.
                if ((!isFirstTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) stalemate++;
                point.x = x - 1; point.y = y - 1;
                if ((!isFirstTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x; point.y = y - 1;
                if ((!isFirstTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x + 1; point.y = y - 1;
                if ((!isFirstTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x - 1; point.y = y;
                if ((!isFirstTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x + 1; point.y = y;
                if ((!isFirstTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x - 1; point.y = y + 1;
                if ((!isFirstTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x; point.y = y + 1;
                if ((!isFirstTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x + 1; point.y = y + 1;
                if ((!isFirstTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
            }
            else if (what < 30) //queen
            {
                for (point.x = x, point.y = y - 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.y--)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isSecondTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x, point.y = y + 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.y++)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isSecondTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x - 1, point.y = y; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x--)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isSecondTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x + 1, point.y = y; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x++)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isSecondTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x - 1, point.y = y - 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x--, point.y--)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isSecondTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x + 1, point.y = y - 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x++, point.y--)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isSecondTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x - 1, point.y = y + 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x--, point.y++)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isSecondTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x + 1, point.y = y + 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x++, point.y++)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isSecondTeam(point.x, point.y)) break;
                    }
                }
            }
            else if (what < 40) //rook
            {
                for (point.x = x, point.y = y - 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.y--)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isSecondTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x, point.y = y + 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.y++)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isSecondTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x - 1, point.y = y; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x--)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isSecondTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x + 1, point.y = y; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x++)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isSecondTeam(point.x, point.y)) break;
                    }
                }
            }
            else if (what < 50) //bishop
            {
                for (point.x = x - 1, point.y = y - 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x--, point.y--)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isSecondTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x + 1, point.y = y - 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x++, point.y--)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isSecondTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x - 1, point.y = y + 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x--, point.y++)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isSecondTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x + 1, point.y = y + 1; (inMap(point.x, point.y) && (!isFirstTeam(point.x, point.y))); point.x++, point.y++)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isSecondTeam(point.x, point.y)) break;
                    }
                }
            }
            else if (what < 60) //knight
            {
                point.x = x - 1; point.y = y - 2;
                if (!isFirstTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x + 1; point.y = y - 2;
                if (!isFirstTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x - 2; point.y = y - 1;
                if (!isFirstTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x + 2; point.y = y - 1;
                if (!isFirstTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x - 2; point.y = y + 1;
                if (!isFirstTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x + 2; point.y = y + 1;
                if (!isFirstTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x - 1; point.y = y + 2;
                if (!isFirstTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x + 1; point.y = y + 2;
                if (!isFirstTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
            }
        }
    }
    //team1검사
    else
    {
        for (iter = team2.piece.begin(); iter != team2.piece.end(); iter++)
        {
            what = iter->what;
            x = iter->where.x;
            y = iter->where.y;
            if (what < 110)//pawn
            {
                if ((!isSecondTeam(x, y - 1)) && (!isSecondTeam(x, y - 1)) && inMap(x, y - 1) && canMove(x, y - 1, what))
                {
                    count++;
                    if ((objectInfo[x][y].first) && (!isSecondTeam(x, y - 2)) && (!isSecondTeam(x, y - 2)) && inMap(x, y - 2) && canMove(x, y - 2, what))
                    {
                        count++;
                    }
                }
                //이동가능경로
                if (isFirstTeam(x - 1, y - 1) && inMap(x - 1, y - 1) && canMove(x - 1, y - 1, what))
                {
                    count++;
                }
                if (isFirstTeam(x + 1, y - 1) && inMap(x + 1, y - 1) && canMove(x + 1, y - 1, what))
                {
                    count++;
                }
                //공격가능경로
            }
            else if (what == 110) //king
            {
                point.x = x; point.y = y;
                if ((!isSecondTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) stalemate++;
                point.x = x - 1; point.y = y - 1;
                if ((!isSecondTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x; point.y = y - 1;
                if ((!isSecondTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x + 1; point.y = y - 1;
                if ((!isSecondTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x - 1; point.y = y;
                if ((!isSecondTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x + 1; point.y = y;
                if ((!isSecondTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x - 1; point.y = y + 1;
                if ((!isSecondTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x; point.y = y + 1;
                if ((!isSecondTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x + 1; point.y = y + 1;
                if ((!isSecondTeam(point.x, point.y)) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                //체크가 되게 움직이면 안됨. 구현요망
            }
            else if (what < 130) //queen
            {
                for (point.x = x, point.y = y - 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.y--)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isFirstTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x, point.y = y + 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.y++)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isFirstTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x - 1, point.y = y; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x--)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isFirstTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x + 1, point.y = y; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x++)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isFirstTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x - 1, point.y = y - 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x--, point.y--)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isFirstTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x + 1, point.y = y - 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x++, point.y--)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isFirstTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x - 1, point.y = y + 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x--, point.y++)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isFirstTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x + 1, point.y = y + 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x++, point.y++)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isFirstTeam(point.x, point.y)) break;
                    }
                }
            }
            else if (what < 140) //rook
            {
                for (point.x = x, point.y = y - 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.y--)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isFirstTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x, point.y = y + 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.y++)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isFirstTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x - 1, point.y = y; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x--)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isFirstTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x + 1, point.y = y; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x++)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isFirstTeam(point.x, point.y)) break;
                    }
                }
            }
            else if (what < 150) //bishop
            {
                for (point.x = x - 1, point.y = y - 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x--, point.y--)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isFirstTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x + 1, point.y = y - 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x++, point.y--)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isFirstTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x - 1, point.y = y + 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x--, point.y++)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isFirstTeam(point.x, point.y)) break;
                    }
                }
                for (point.x = x + 1, point.y = y + 1; (inMap(point.x, point.y) && (!isSecondTeam(point.x, point.y))); point.x++, point.y++)
                {
                    if (canMove(point.x, point.y, what))
                    {
                        count++;
                        if (isFirstTeam(point.x, point.y)) break;
                    }
                }
            }
            else if (what < 160) //knight
            {
                point.x = x - 1; point.y = y - 2;
                if (!isSecondTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x + 1; point.y = y - 2;
                if (!isSecondTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x - 2; point.y = y - 1;
                if (!isSecondTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x + 2; point.y = y - 1;
                if (!isSecondTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x - 2; point.y = y + 1;
                if (!isSecondTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x + 2; point.y = y + 1;
                if (!isSecondTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x - 1; point.y = y + 2;
                if (!isSecondTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
                point.x = x + 1; point.y = y + 2;
                if (!isSecondTeam(point.x, point.y) && inMap(point.x, point.y) && canMove(point.x, point.y, what)) count++;
            }
        }
    }
    //team2검사
    if ((count == 0) && (stalemate == 1))
    {
        drawCount = 50; //스테일메이트 상태라면 비기게 만든다.
    }
    else if (count == 0)
    {
        if (whosTurn)
        {
            whoIsWin = 1;   //click()함수에서 서로 turn이 바뀌었기 때문에 바껴야됨.
        }
        else
        {
            whoIsWin = 0;
        }
        PlaySound(TEXT("chessSound\\victory.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }
}

void QueensideCastling()
{
    vector<CHESSPIECE>::iterator iter;
    if (whosTurn)
    {
        for (iter = team1.piece.begin(); iter != team1.piece.end(); iter++)
        {
            if (iter->what == 30)
            {
                iter->where.x = 3;
                iter->where.y = 0;
                break;
            }
        }
    }
    else
    {
        for (iter = team2.piece.begin(); iter != team2.piece.end(); iter++)
        {
            if (iter->what == 130)
            {
                iter->where.x = 3;
                iter->where.y = 7;
                break;
            }
        }
    }
}

void KingsideCastling()
{
    vector<CHESSPIECE>::iterator iter;
    if (whosTurn)
    {
        for (iter = team1.piece.begin(); iter != team1.piece.end(); iter++)
        {
            if (iter->what == 31)
            {
                iter->where.x = 5;
                iter->where.y = 0;
                break;
            }
        }
    }
    else
    {
        for (iter = team2.piece.begin(); iter != team2.piece.end(); iter++)
        {
            if (iter->what == 131)
            {
                iter->where.x = 5;
                iter->where.y = 7;
                break;
            }
        }
    }
}

void drawBackground()
{
    for (int i = 0; i < 8; i++)
        for (int k = 0; k < 8; k++)
        {
            if ((i + k) % 2 == 0)
                SelectObject(backMemDC, evenBrush);
            else
                SelectObject(backMemDC, oddBrush);
            Rectangle(backMemDC, k * SIZE, i * SIZE, k * SIZE + SIZE, i * SIZE + SIZE);
        }
    SelectObject(backMemDC, infoBrush);
    Rectangle(backMemDC, 8 * SIZE, 0, 10 * SIZE, 8 * SIZE);
}

void drawLog()
{
    if (selectObject[0][0].x != -1)
    {
        SelectObject(backMemDC, selectBrush);
        Rectangle(backMemDC, selectObject[0][0].x * SIZE, selectObject[0][0].y * SIZE, selectObject[0][0].x * SIZE + SIZE, selectObject[0][0].y * SIZE + SIZE);
        if (selectObject[0][1].x != -1)
            Rectangle(backMemDC, selectObject[0][1].x * SIZE, selectObject[0][1].y * SIZE, selectObject[0][1].x * SIZE + SIZE, selectObject[0][1].y * SIZE + SIZE);
    }
    if (selectObject[1][0].x != -1)
    {
        SelectObject(backMemDC, selectBrush);
        Rectangle(backMemDC, selectObject[1][0].x * SIZE, selectObject[1][0].y * SIZE, selectObject[1][0].x * SIZE + SIZE, selectObject[1][0].y * SIZE + SIZE);
        if (selectObject[1][1].x != -1)
            Rectangle(backMemDC, selectObject[1][1].x * SIZE, selectObject[1][1].y * SIZE, selectObject[1][1].x * SIZE + SIZE, selectObject[1][1].y * SIZE + SIZE);
    }
}

void drawObjects(Graphics* g)
{
    vector<CHESSPIECE>::iterator iter;
    for (iter = team1.piece.begin(); iter != team1.piece.end(); iter++)
    {
        g->DrawImage(team1.image[(iter->what) / 10], iter->where.x * SIZE, iter->where.y * SIZE, SIZE, SIZE);
    }
    //1팀 기물 출력
    for (iter = team2.piece.begin(); iter != team2.piece.end(); iter++)
    {
        g->DrawImage(team2.image[(iter->what - 100) / 10], iter->where.x * SIZE, iter->where.y * SIZE, SIZE, SIZE);
    }
    //2팀 기물 출력
    if (promotion)
    {
        SelectObject(backMemDC, selectBrush);
        Rectangle(backMemDC, promotionPoint.x * SIZE, promotionPoint.y * SIZE, promotionPoint.x * SIZE + SIZE, promotionPoint.y * SIZE + SIZE);
        if (whosTurn == true)
        {
            g->DrawImage(team1.image[2], promotionPoint.x * SIZE, promotionPoint.y * SIZE, SIZE / 2, SIZE / 2);
            g->DrawImage(team1.image[3], promotionPoint.x * SIZE + SIZE / 2, promotionPoint.y * SIZE, SIZE / 2, SIZE / 2);
            g->DrawImage(team1.image[4], promotionPoint.x * SIZE, promotionPoint.y * SIZE + SIZE / 2, SIZE / 2, SIZE / 2);
            g->DrawImage(team1.image[5], promotionPoint.x * SIZE + SIZE / 2, promotionPoint.y * SIZE + SIZE / 2, SIZE / 2, SIZE / 2);
        }
        else if (whosTurn == false)
        {
            g->DrawImage(team2.image[2], promotionPoint.x * SIZE, promotionPoint.y * SIZE, SIZE / 2, SIZE / 2);
            g->DrawImage(team2.image[3], promotionPoint.x * SIZE + SIZE / 2, promotionPoint.y * SIZE, SIZE / 2, SIZE / 2);
            g->DrawImage(team2.image[4], promotionPoint.x * SIZE, promotionPoint.y * SIZE + SIZE / 2, SIZE / 2, SIZE / 2);
            g->DrawImage(team2.image[5], promotionPoint.x * SIZE + SIZE / 2, promotionPoint.y * SIZE + SIZE / 2, SIZE / 2, SIZE / 2);
        }
    }
    //프로모션이라면 무엇을 선택할지 선택하는것을 보여줌
}

void drawCanGo(Graphics* g)
{
    vector<POINT>::iterator pointIter;
    SolidBrush b1(Color(100, 50, 50, 50));
    for (pointIter = youCanGo.begin(); pointIter != youCanGo.end(); pointIter++)
    {
        g->FillEllipse(&b1, pointIter->x * SIZE + (SIZE / 4), pointIter->y * SIZE + (SIZE / 4), SIZE / 2, SIZE / 2);
    }
}

void drawPow(Graphics* g)
{
    if (powinfo.time < 1.0)
    {
        powinfo.time += g_fDeltatime;
        g->DrawImage(powinfo.powImage, powinfo.where.x * SIZE, powinfo.where.y * SIZE, SIZE, SIZE);
    }
}

void drawInfo(Graphics* g)
{
    TCHAR info[30];
    
    //CreateFont때문에 그런건가? 메모리 용량이 늘어나는거일수도.
    SetBkMode(backMemDC, TRANSPARENT);
    SelectObject(backMemDC, hFont);
    if (whosTurn)
    {
        SetTextColor(backMemDC, RGB(255, 255, 255));
        wsprintf(info, L"PLAYER 1 차례");
        TextOut(backMemDC, SIZE * 8, 20, info, lstrlen(info));
    }
    else
    {
        SetTextColor(backMemDC, RGB(0, 0, 0));
        wsprintf(info, L"PLAYER 2 차례");
        TextOut(backMemDC, SIZE * 8, 20, info, lstrlen(info));
    }
    //누구의 차례인지 보이기
    
    vector<CHESSPIECE>::iterator iter;
    int i = 0;
    for (iter = team1.attackedPiece.begin(); iter != team1.attackedPiece.end(); iter++, i++)
    {
        g->DrawImage(team1.image[(iter->what) / 10], SIZE * 8, SIZE / 2 + SIZE * i / 2, SIZE, SIZE);
    }
    i = 0;
    for (iter = team2.attackedPiece.begin(); iter != team2.attackedPiece.end(); iter++, i++)
    {
        g->DrawImage(team2.image[(iter->what - 100) / 10], SIZE * 9, SIZE / 2 + SIZE * i / 2, SIZE, SIZE);
    }
    //공격당한 기물들 보이기

    SetBkMode(backMemDC, TRANSPARENT);
    SelectObject(backMemDC, hFont);
    SetTextColor(backMemDC, RGB(255, 0, 0));
    if (Kingcheck == 1)
    {
        wsprintf(info, L"PLAYER 1 체크!");
        TextOut(backMemDC, SIZE * 8, 50, info, lstrlen(info));
    }
    else if (Kingcheck == 2)
    {
        wsprintf(info, L"PLAYER 2 체크!");
        TextOut(backMemDC, SIZE * 8, 50, info, lstrlen(info));
    }
    //체크상태이면 체크 보이기

    if (gameReady == 2)
    {
        if (playerNum == 1)
        {
            SetBkMode(backMemDC, TRANSPARENT);
            SelectObject(backMemDC, hFont);
            SetTextColor(backMemDC, RGB(255, 255, 255));
            wsprintf(info, L"나: 플레이어1");
            TextOut(backMemDC, SIZE * 8, 760, info, lstrlen(info));
        }
        else if (playerNum == 2)
        {
            SetBkMode(backMemDC, TRANSPARENT);
            SelectObject(backMemDC, hFont);
            SetTextColor(backMemDC, RGB(0, 0, 0));
            wsprintf(info, L"나: 플레이어2");
            TextOut(backMemDC, SIZE * 8, 760, info, lstrlen(info));
        }
    }
    
    //wsprintf(info, L"%d %d %d", playerNum ,ptMouse.x, ptMouse.y);
    //TextOut(backMemDC, SIZE * 8, 50, info, lstrlen(info));
}

void winOrDraw()
{
    if (whoIsWin != -1)
    {
        TCHAR info[30];
        HFONT hFont = CreateFont(43, 0, 0, 0, 0, 0, 0, 0, HANGEUL_CHARSET, 0, 0, 0,
            VARIABLE_PITCH | FF_ROMAN, TEXT("HY견고딕"));
        SetBkMode(backMemDC, TRANSPARENT);
        SelectObject(backMemDC, hFont);
        if (whoIsWin == 0)
        {
            //MessageBox(NULL, L"1player가 이겼습니다! 축하드립니다!!", L"1player 승리", MB_OK);
            wsprintf(info, L"1player가 이겼습니다! 축하드립니다!!");
            SetTextColor(backMemDC, RGB(255, 255, 255));
            TextOut(backMemDC, 0, SIZE * 4, info, lstrlen(info));
        }
        else
        {
            //MessageBox(NULL, L"2player가 이겼습니다! 축하드립니다!!", L"2player 승리", MB_OK);
            wsprintf(info, L"2player가 이겼습니다! 축하드립니다!!");
            SetTextColor(backMemDC, RGB(0, 0, 0));
            TextOut(backMemDC, 0, SIZE * 4, info, lstrlen(info));
        }
        stillContinue = false;
    }
    if (drawCount >= 50)
    {
        TCHAR info[30];
        HFONT hFont = CreateFont(60, 0, 0, 0, 0, 0, 0, 0, HANGEUL_CHARSET, 0, 0, 0,
            VARIABLE_PITCH | FF_ROMAN, TEXT("HY견고딕"));
        SetBkMode(backMemDC, TRANSPARENT);
        SelectObject(backMemDC, hFont);
        wsprintf(info, L"무승부");
        TextOut(backMemDC, SIZE * 3, SIZE * 4, info, lstrlen(info));
        stillContinue = false;
    }
}

void Error(LPCWSTR m) {
    MessageBox(NULL, m, m, MB_OK);
    exit(1);
}

void multiRun()
{
    static Graphics g(backMemDC);
    static TCHAR str[30];

    timeCount();

    if (click && stillContinue)
    {
        click = false;
        multiClicked();
    }

    drawBackground();   //배경화면 출력
    drawLog();          //상대방이 움직였던 것, 내가 클릭한것 출력
    drawObjects(&g);    //체스 말 출력
    drawCanGo(&g);      //갈 수 있는곳 원으로 표시
    drawPow(&g);
    drawInfo(&g);
    winOrDraw();

    TCHAR info[30];
    wsprintf(info, L"%d %d", enPassant, castling);
    BitBlt(g_hDC, 0, 0, SIZE * 10, SIZE * 8, backMemDC, 0, 0, SRCCOPY);

    recvData();     //상대 플레이어 차례일경우, recv를 기다려 어디를 click했는지 알아낸다.
}

void multiClicked()
{
    int x = ptMouse.x / SIZE;
    int y = ptMouse.y / SIZE;
    if (inMap(x, y) == false)
        return;
    static int what;       //현재 클릭된것이 무엇인지 저장하기 위한 변수
    if (promotion)
        promotionDo(&what);
    //프로모션 우선처리
    else
    {
        changePlayer = 0;
        if (youCanGo.size() != 0)   //이동할 기물을 클릭했을 상태에서 이동가능함
            objectMove(&what);
        //기물 이동. 갈수있는곳을 클릭한다면 이동. 갈수없는곳을 클릭한다면 선택해제
        if (youCanGo.size() == 0)   //갈수 있는곳이 없다면 기물을 선택해야됨. 선택한 기물의 갈수있는곳을 알려줌.
            objectSelect(&what);
        //기물을 선택하면 갈 수 있는곳을 표시함.
    }
    sendData();
}

void sendData()
{
    if (changePlayer == 1)  //차례가 바뀌었다
    {
        if ((movedPlayer == 1) && (playerNum == 1) || (movedPlayer == 2) && (playerNum == 2))
            //그런데 자기 플레이어num의 기물이 움직여야 send
        {
            char message[30] = { 0, };
            sprintf(message, "%3d %3d %d %d", ptMouse.x, ptMouse.y, playerNum, changePlayer);
            send(hSock, message, strlen(message), 0);
            changePlayer = 0;
        }
    }
    
    if ((whosTurn == true) && (playerNum == 1) && (changePlayer == 0))
    {
        char message[30] = { 0, };
        sprintf(message, "%3d %3d %d %d", ptMouse.x, ptMouse.y, playerNum, changePlayer);
        send(hSock, message, strlen(message), 0);
    }
    else if ((whosTurn == false) && (playerNum == 2) && (changePlayer == 0))
    {
        char message[30] = { 0, };
        sprintf(message, "%3d %3d %d %d", ptMouse.x, ptMouse.y, playerNum, changePlayer);
        send(hSock, message, strlen(message), 0);
    }
    
}

void recvData()
{
    char message[30] = { 0, };
    TCHAR info[30];
    int strLen;
    if ((whosTurn == false) && (playerNum == 1))    //자신은 1플레이어인데 2플레이어차례인경우. 좌표가 올때까지 기다린다.
    {
        strLen = recv(hSock, message, sizeof(message) - 1, 0);
        if (strLen != SOCKET_ERROR)   //전송받는데 이상이 없었다면
        {
            //recvPlayerNum = message[8];
            char x[5] = { 0, };
            x[0] = message[0]; x[1] = message[1]; x[2] = message[2];
            char y[5] = { 0, };
            y[0] = message[4]; y[1] = message[5]; y[2] = message[6];
            ptMouse.x = atoi(x);
            ptMouse.y = atoi(y);
            click = true;
        }
    }
    else if ((whosTurn == true) && (playerNum == 2))
    {
        strLen = recv(hSock, message, sizeof(message) - 1, 0);
        if (strLen != SOCKET_ERROR)
        {
            //recvPlayerNum = message[8];
            char x[5] = { 0, };
            x[0] = message[0]; x[1] = message[1]; x[2] = message[2];
            char y[5] = { 0, };
            y[0] = message[4]; y[1] = message[5]; y[2] = message[6];
            ptMouse.x = atoi(x);
            ptMouse.y = atoi(y);
            click = true;
        }
    }
}