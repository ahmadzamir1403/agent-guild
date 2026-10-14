#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include "model.h"

#define WM_MODEL (WM_APP+1)
static HWND window;
static CRITICAL_SECTION lock;
static Model model;
static HANDLE stop_event,worker_handle;
static HANDLE instance_mutex;
static int probe_mode;
static int scroll_row=0,filter=0;
static wchar_t selected_id[100];
static HFONT fonts[7];
static HDC back_dc;
static HBITMAP back_bitmap,back_old;
static int back_width,back_height;
static RECT row_hits[10],filter_hits[3],refresh_hit;
static int row_indices[10],row_count;
static COLORREF BG=RGB(14,17,24), PANEL=RGB(22,27,37), LINE=RGB(39,46,61);
static COLORREF INK=RGB(238,241,246), MUTED=RGB(144,155,174), MINT=RGB(128,237,187), GOLD=RGB(255,205,118), LILAC=RGB(181,165,250);
static const wchar_t *states[]={L"ON A QUEST",L"NEEDS YOU",L"AT CAMP",L"RESTING",L"ERROR",L"UNKNOWN"};
static const wchar_t *state_desc[]={L"Codex reports an active turn.",L"Codex is waiting for a response or approval.",L"Loaded and ready for another quest.",L"Stored thread; not loaded on this host.",L"Codex reports a system error.",L"The host has not supplied a known state."};
static void publish(Model *next) {
    EnterCriticalSection(&lock);
    model=*next;
    if(!selected_id[0] && model.count) wcscpy(selected_id,model.agents[0].id);
    LeaveCriticalSection(&lock);
    PostMessage(window,WM_MODEL,0,0);
}
static void connection_message(const wchar_t *msg) {
    EnterCriticalSection(&lock); model.connected=0;
    wcsncpy(model.message,msg,239); model.message[239]=0;
    LeaveCriticalSection(&lock); PostMessage(window,WM_MODEL,0,0);
}

#include "transport.h"

static COLORREF state_color(int state) {
    return state==WORKING?MINT:state==WAITING?GOLD:state==IDLE?LILAC:state==FAILED?RGB(255,136,144):MUTED;
}
static void fill(HDC dc,int x,int y,int w,int h,COLORREF c) {
    RECT r={x,y,x+w,y+h};SetDCBrushColor(dc,c);FillRect(dc,&r,(HBRUSH)GetStockObject(DC_BRUSH));
}
static void panel(HDC dc,int x,int y,int w,int h,COLORREF c,COLORREF edge,int radius) {
    HBRUSH b=CreateSolidBrush(c); HPEN p=CreatePen(PS_SOLID,1,edge);
    HGDIOBJ ob=SelectObject(dc,b),op=SelectObject(dc,p);
    RoundRect(dc,x,y,x+w,y+h,radius,radius); SelectObject(dc,ob); SelectObject(dc,op); DeleteObject(b); DeleteObject(p);
}
static void label(HDC dc,int x,int y,int w,int h,const wchar_t *s,int font,COLORREF color,UINT style) {
    RECT r={x,y,x+w,y+h}; SelectObject(dc,fonts[font]); SetTextColor(dc,color); SetBkMode(dc,TRANSPARENT);
    DrawTextW(dc,s,-1,&r,DT_NOPREFIX|style);
}
static void dot(HDC dc,int x,int y,COLORREF c) {
    HBRUSH b=CreateSolidBrush(c); HGDIOBJ old=SelectObject(dc,b); HPEN p=CreatePen(PS_SOLID,1,c),op=SelectObject(dc,p);
    Ellipse(dc,x,y,x+7,y+7); SelectObject(dc,old); SelectObject(dc,op); DeleteObject(b); DeleteObject(p);
}
static unsigned int id_hash(const wchar_t *s) { unsigned int h=2166136261u; while(*s) h=(h^*s++)*16777619u; return h; }
#include "world.h"
/* Code-drawn pixel adventurers: no textures or external assets. */
static void avatar(HDC dc,int x,int y,int size,unsigned int seed,COLORREF accent) {
    static const char *pixels[]={"00111100","01111110","11011011","11111111","01111110","00111100","01100110","11000011"};
    int r,c,p=size/8;
    for(r=0;r<8;r++) for(c=0;c<8;c++) if(pixels[r][c]=='1') {
        COLORREF color=r==2?RGB(222,233,242):r>4?RGB(69,81,108):accent;
        if(r==0 && (seed&1)) color=LILAC;
        fill(dc,x+c*p,y+r*p,p,p,color);
    }
}
static void sword(HDC dc,int x,int y,int p,COLORREF accent) {
    int i;
    for(i=0;i<7;i++) fill(dc,x+i*p,y+(6-i)*p,p*2,p*2,RGB(214,227,239));
    fill(dc,x+p,y+p*6,p*3,p,accent); fill(dc,x+p*2,y+p*5,p,p*3,accent);
    fill(dc,x,y+p*8,p*2,p*2,RGB(126,104,81));
}
static const wchar_t *basename_w(const wchar_t *p) {
    const wchar_t *result=p,*t=p;
    while(*t) { if((*t=='\\'||*t=='/') && t[1]) result=t+1; t++; }
    return *result?result:L"No project supplied";
}
static int visible(const Agent *a) {
    if(a->state==OFFLINE) return 0;
    return !filter || (filter==1 && a->state==WORKING) || (filter==2 && a->state==WAITING);
}
static void draw_roster(HDC dc) {
    int i,n=0,working=0,waiting=0,idle=0,shown=0,visible_count=0;
    int indices[MAX_AGENTS]; wchar_t text[300]; Agent *a=NULL;
    fill(dc,0,0,1180,800,BG);
    fill(dc,0,0,1180,3,MINT);
    panel(dc,28,26,40,40,RGB(35,57,48),RGB(53,85,68),10);
    avatar(dc,36,34,24,0,MINT);
    label(dc,82,23,350,32,L"AGENT GUILD",2,INK,DT_SINGLELINE);
    label(dc,83,55,460,20,L"YOUR AI PARTY, AT A GLANCE",0,MUTED,DT_SINGLELINE);
    panel(dc,900,28,250,40,PANEL,LINE,10);
    dot(dc,916,44,model.connected?MINT:GOLD);
    label(dc,934,38,200,24,model.connected?L"LIVE CODEX HOST":L"CONNECTING TO CODEX",0,model.connected?MINT:GOLD,DT_SINGLELINE);
    fill(dc,28,91,1124,1,LINE);
    label(dc,28,114,800,24,L"THE QUEST BOARD",0,MINT,DT_SINGLELINE);
    label(dc,27,142,900,46,L"A little party. A lot of possibility.",3,INK,DT_SINGLELINE);
    label(dc,29,197,900,24,L"See who's adventuring, who's resting, and who needs a hand.",1,MUTED,DT_SINGLELINE);
    sword(dc,1030,130,7,MINT);
    for(i=0;i<model.count;i++) {
        int s=model.agents[i].state;
        if(s==WORKING) working++; if(s==WAITING) waiting++; if(s==IDLE) idle++;
        if(visible(&model.agents[i])) indices[visible_count++]=i;
    }
    {
        int values[3]={working,waiting,idle};
        const wchar_t *titles[3]={L"On a quest",L"Needs your help",L"At camp"};
        const wchar_t *subtitles[3]={L"Active turns",L"Waiting on a response",L"Idle, loaded agents"};
        COLORREF colors[3]={MINT,GOLD,LILAC};
        for(i=0;i<3;i++) {
            int x=28+i*382;
            panel(dc,x,241,360,110,PANEL,LINE,14);
            dot(dc,x+22,262,colors[i]);
            label(dc,x+39,252,260,26,titles[i],1,MUTED,DT_SINGLELINE);
            if(model.connected) _snwprintf(text,299,L"%02d",values[i]); else wcscpy(text,L"--");
            label(dc,x+21,283,100,50,text,4,colors[i],DT_SINGLELINE);
            label(dc,x+111,306,220,25,subtitles[i],0,MUTED,DT_SINGLELINE);
        }
    }
    label(dc,28,375,320,32,L"Your party",2,INK,DT_SINGLELINE);
    for(i=0;i<3;i++) {
        int x=463+i*91;
        const wchar_t *names[3]={L"All",L"Working",L"Waiting"};
        filter_hits[i]=(RECT){x,374,x+83,408};
        panel(dc,x,374,83,34,filter==i?RGB(37,62,52):PANEL,filter==i?RGB(61,105,82):LINE,10);
        label(dc,x,382,83,21,names[i],0,filter==i?MINT:MUTED,DT_SINGLELINE|DT_CENTER);
    }
    if(scroll_row>visible_count-1) scroll_row=visible_count>0?visible_count-1:0;
    if(scroll_row<0) scroll_row=0;
    row_count=0;
    for(i=scroll_row;i<visible_count && shown<4;i++,shown++) {
        int index=indices[i],y=426+shown*77;
        Agent *item=&model.agents[index]; COLORREF c=state_color(item->state);
        int chosen=!wcscmp(item->id,selected_id);
        panel(dc,28,y,700,66,chosen?RGB(27,41,39):PANEL,chosen?RGB(66,114,91):LINE,12);
        if(chosen) fill(dc,29,y+13,3,40,MINT);
        panel(dc,42,y+10,46,46,RGB(30,37,50),RGB(43,52,68),10);
        avatar(dc,49,y+17,32,id_hash(item->id),c);
        label(dc,105,y+10,450,25,item->name,5,INK,DT_SINGLELINE|DT_END_ELLIPSIS);
        _snwprintf(text,299,L"%ls  /  %ls",item->child?L"SUBAGENT":L"AGENT",basename_w(item->cwd));
        text[299]=0; label(dc,106,y+37,450,18,text,0,MUTED,DT_SINGLELINE|DT_END_ELLIPSIS);
        dot(dc,578,y+29,c); label(dc,594,y+23,120,23,states[item->state],0,c,DT_SINGLELINE);
        row_hits[row_count]=(RECT){28,y,728,y+66}; row_indices[row_count++]=index;
    }
    if(!shown) {
        panel(dc,28,426,700,297,PANEL,LINE,14);
        avatar(dc,339,465,64,1,model.connected?LILAC:GOLD);
        label(dc,48,552,660,35,model.connected?L"The camp is quiet.":L"Finding your party...",2,INK,DT_SINGLELINE|DT_CENTER);
        label(dc,68,601,620,72,model.connected?L"No agents match this view. Start a Codex task and your party will appear here.":model.message,1,MUTED,DT_WORDBREAK|DT_CENTER);
    }
    panel(dc,752,374,400,349,PANEL,LINE,14);
    label(dc,777,394,350,23,L"QUEST INSPECTOR",0,MUTED,DT_SINGLELINE);
    for(i=0;i<model.count;i++) if(!wcscmp(model.agents[i].id,selected_id)) { a=&model.agents[i]; break; }
    if(a && a->state!=OFFLINE) {
        COLORREF c=state_color(a->state);
        avatar(dc,777,438,48,id_hash(a->id),c);
        label(dc,845,439,280,25,a->child?L"Party companion":L"Quest leader",5,INK,DT_SINGLELINE);
        label(dc,845,471,280,22,states[a->state],0,c,DT_SINGLELINE);
        label(dc,777,516,350,65,a->name,2,INK,DT_WORDBREAK|DT_END_ELLIPSIS);
        label(dc,777,593,350,55,model.connected?state_desc[a->state]:L"Disconnected. Last known state may be stale.",1,model.connected?MUTED:GOLD,DT_WORDBREAK);
        label(dc,777,658,350,20,basename_w(a->cwd),0,MUTED,DT_SINGLELINE|DT_END_ELLIPSIS);
        if(a->detail[0]) label(dc,777,686,350,20,a->detail,0,GOLD,DT_SINGLELINE|DT_END_ELLIPSIS);
    } else {
        avatar(dc,777,440,48,5,LILAC);
        label(dc,777,514,350,55,L"Every quest needs a party.",2,INK,DT_WORDBREAK);
        label(dc,777,584,350,90,L"Select an agent to inspect its quest and current state. Status comes directly from your Codex host.",1,MUTED,DT_WORDBREAK);
    }
    _snwprintf(text,299,L"%d visible  /  %d loaded  /  scroll to explore",visible_count,model.count);
    if(model.truncated) wcscat(text,L"  /  limited coverage");
    label(dc,30,731,900,25,text,0,MUTED,DT_SINGLELINE);
    fill(dc,28,763,1124,1,LINE);
    _snwprintf(text,299,L"%ls  /  %ls",model.connected?L"LOCAL HOST":L"HOST UNAVAILABLE",model.connected?L"Read-only polling - no inference calls":model.message);
    text[299]=0; label(dc,29,775,940,22,text,0,model.connected?MUTED:GOLD,DT_SINGLELINE|DT_END_ELLIPSIS);
    refresh_hit=(RECT){1014,769,1152,798};
    label(dc,1014,775,138,22,L"Refresh  [R]",0,MINT,DT_SINGLELINE|DT_RIGHT);
}
#include "arena.h"
static void animation_timer(HWND hwnd) {
    int interval=100,i;
    EnterCriticalSection(&lock);if(model.connected)for(i=0;i<model.count;i++)if(model.agents[i].state==WORKING){interval=50;break;}LeaveCriticalSection(&lock);
    last_frame=GetTickCount();
    if(world_view&&!paused&&!IsIconic(hwnd))SetTimer(hwnd,1,interval,NULL);else KillTimer(hwnd,1);
}
static LRESULT CALLBACK wndproc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_PAINT) {
        PAINTSTRUCT ps;RECT r;HDC dc=BeginPaint(hwnd,&ps);
        GetClientRect(hwnd,&r);if(r.right<=0||r.bottom<=0){EndPaint(hwnd,&ps);return 0;}
        if(!back_dc)back_dc=CreateCompatibleDC(dc);
        if(!back_bitmap||r.right!=back_width||r.bottom!=back_height) {
            if(back_bitmap){SelectObject(back_dc,back_old);DeleteObject(back_bitmap);}
            back_bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);back_old=SelectObject(back_dc,back_bitmap);back_width=r.right;back_height=r.bottom;
        }
        SetMapMode(back_dc,MM_ANISOTROPIC); SetWindowExtEx(back_dc,1180,800,NULL); SetViewportExtEx(back_dc,r.right,r.bottom,NULL);
        EnterCriticalSection(&lock);draw(back_dc);LeaveCriticalSection(&lock);
        SetMapMode(back_dc,MM_TEXT);BitBlt(dc,0,0,r.right,r.bottom,back_dc,0,0,SRCCOPY);
        EndPaint(hwnd,&ps);return 0;
    }
    if(msg==WM_ERASEBKGND) return 1;
    if(msg==WM_MODEL) {EnterCriticalSection(&lock);world_sync();LeaveCriticalSection(&lock);economy_save(0);animation_timer(hwnd);InvalidateRect(hwnd,NULL,FALSE);return 0;}
    if(msg==WM_SIZE) {animation_timer(hwnd);InvalidateRect(hwnd,NULL,FALSE);return 0;}
    if(msg==WM_TIMER&&wp==1) {
        DWORD now=GetTickCount();float dt=(now-last_frame)/1000.0f;last_frame=now;
        EnterCriticalSection(&lock);if(!paused)world_step(dt);LeaveCriticalSection(&lock);economy_save(0);
        if(world_view)InvalidateRect(hwnd,NULL,FALSE);return 0;
    }
    if(msg==WM_GETMINMAXINFO) { MINMAXINFO *m=(MINMAXINFO*)lp; m->ptMinTrackSize.x=940; m->ptMinTrackSize.y=690; return 0; }
    if(msg==WM_LBUTTONDOWN) {
        RECT r; POINT p; int i; GetClientRect(hwnd,&r);
        p.x=(short)LOWORD(lp)*1180/r.right; p.y=(short)HIWORD(lp)*800/r.bottom;
        EnterCriticalSection(&lock);
        if(world_view)for(i=hero_hit_count-1;i>=0;i--)if(PtInRect(&hero_hits[i],p)){wcscpy(selected_id,heroes[hero_slots[i]].id);break;}
        for(i=0;i<row_count;i++) if(PtInRect(&row_hits[i],p)) wcscpy(selected_id,model.agents[row_indices[i]].id);
        for(i=0;i<3;i++) if(PtInRect(&filter_hits[i],p)) { filter=i; scroll_row=0; }
        for(i=0;i<2;i++)if(PtInRect(&view_hits[i],p)){world_view=i==0;scroll_row=0;}
        if(world_view&&PtInRect(&pause_hit,p))paused=!paused;
        LeaveCriticalSection(&lock);animation_timer(hwnd);if(PtInRect(&refresh_hit,p))SetEvent(refresh_event);InvalidateRect(hwnd,NULL,FALSE);return 0;
    }
    if(msg==WM_MOUSEWHEEL) { scroll_row-=(short)HIWORD(wp)/WHEEL_DELTA; if(scroll_row<0) scroll_row=0; InvalidateRect(hwnd,NULL,FALSE); return 0; }
    if(msg==WM_KEYDOWN && (wp==VK_DOWN || wp==VK_UP)) { scroll_row+=wp==VK_DOWN?1:-1; if(scroll_row<0)scroll_row=0; InvalidateRect(hwnd,NULL,FALSE); return 0; }
    if(msg==WM_KEYDOWN && wp=='R') { SetEvent(refresh_event); return 0; }
    if(msg==WM_KEYDOWN && wp==VK_SPACE) {paused=!paused;animation_timer(hwnd);InvalidateRect(hwnd,NULL,FALSE);return 0;}
    if(msg==WM_CLOSE) { DestroyWindow(hwnd); return 0; }
    if(msg==WM_DESTROY) {KillTimer(hwnd,1);economy_save(1);SetEvent(stop_event);PostQuitMessage(0);return 0;}
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static int self_test(void) {
    const char *s="{\"result\":{\"data\":[{\"id\":\"a\",\"name\":\"Quest \\u4f60\\u597d \\ud83d\\ude80\",\"status\":{\"type\":\"active\",\"activeFlags\":[\"waitingOnApproval\"]},\"source\":{\"subAgent\":{\"thread_spawn\":{\"parent_thread_id\":\"root\"}}}},{\"id\":\"b\",\"status\":{\"type\":\"idle\"}},{\"id\":\"c\",\"status\":{\"type\":\"notLoaded\"}}]}}";
    Json j; Agent a; int data,i;
    if(!json_open(&j,s))return 1;
    data=member(&j,member(&j,0,"result"),"data"); i=data+1;
    parse_agent(&j,i,&a);
    if(a.state!=WAITING||!a.child||wcscmp(a.parent,L"root")||a.name[6]!=0x4f60||a.name[9]!=0xd83d){free(j.t);return 2;}
    i=skip(&j,i);parse_agent(&j,i,&a);if(a.state!=IDLE){free(j.t);return 3;}
    i=skip(&j,i);parse_agent(&j,i,&a);if(a.state!=OFFLINE){free(j.t);return 4;}
    free(j.t);if(json_open(&j,"{\"broken\":")){free(j.t);return 5;}
    if(json_open(&j,"{} {}")){free(j.t);return 6;}
    if(!json_open(&j,"{\"id\":\"private\",\"preview\":\"private prompt text\"}"))return 7;
    parse_agent(&j,0,&a);free(j.t);if(wcscmp(a.name,L"Untitled quest"))return 8;
    return 0;
}
static void artifact_path(wchar_t *path,const wchar_t *name) {
    wchar_t *last;GetModuleFileNameW(NULL,path,520);last=wcsrchr(path,L'\\');if(last)last[1]=0;else path[0]=0;wcscat(path,name);
}
static int diagnostics(void) {
    int i,counts[6]={0},children=0,result;char report[400];wchar_t path[520];
    HANDLE file;DWORD written;HDC screen,dc;BITMAPINFO info;BITMAPFILEHEADER header;HBITMAP bmp,old;void *pixels;
    probe_mode=1;worker_handle=CreateThread(NULL,0,worker,NULL,0,NULL);
    if(!worker_handle)return 30;
    WaitForSingleObject(worker_handle,12000);SetEvent(stop_event);WaitForSingleObject(worker_handle,6000);
    EnterCriticalSection(&lock);
    world_sync();for(i=0;i<450;i++)world_step(1.0f/30.0f);
    for(i=0;i<model.count;i++){counts[model.agents[i].state]++;if(model.agents[i].child)children++;}
    snprintf(report,sizeof(report),"{\"connected\":%s,\"loaded\":%d,\"working\":%d,\"waiting\":%d,\"idle\":%d,\"subagents\":%d,\"truncated\":%s}\n",model.connected?"true":"false",model.count,counts[WORKING],counts[WAITING],counts[IDLE],children,model.truncated?"true":"false");
    artifact_path(path,L"probe.json");file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file!=INVALID_HANDLE_VALUE){WriteFile(file,report,(DWORD)strlen(report),&written,NULL);CloseHandle(file);}
    screen=GetDC(NULL);dc=CreateCompatibleDC(screen);memset(&info,0,sizeof(info));
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=1180;info.bmiHeader.biHeight=-800;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    bmp=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,NULL,0);
    if(bmp) {
        old=SelectObject(dc,bmp);draw(dc);memset(&header,0,sizeof(header));header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+1180*800*4;
        artifact_path(path,L"preview.bmp");file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
        if(file!=INVALID_HANDLE_VALUE) {WriteFile(file,&header,sizeof(header),&written,NULL);WriteFile(file,&info.bmiHeader,sizeof(BITMAPINFOHEADER),&written,NULL);WriteFile(file,pixels,1180*800*4,&written,NULL);CloseHandle(file);}
        SelectObject(dc,old);DeleteObject(bmp);
    }
    DeleteDC(dc);ReleaseDC(NULL,screen);result=model.connected?0:20;LeaveCriticalSection(&lock);
    CloseHandle(worker_handle);return result;
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR cmd,int show) {
    WNDCLASSW wc; MSG msg; int i;
    if(strstr(cmd,"--self-test")){int result=self_test();if(!result)result=world_test();return result?result:economy_file_test();}
    if(!strstr(cmd,"--probe")) {
        instance_mutex=CreateMutexW(NULL,FALSE,L"Local\\AgentGuildNative");
        if(instance_mutex&&GetLastError()==ERROR_ALREADY_EXISTS) {HWND existing=FindWindowW(L"AgentGuildNative",L"Agent Guild");if(existing){ShowWindow(existing,SW_RESTORE);SetForegroundWindow(existing);}CloseHandle(instance_mutex);return 0;}
        economy_load();
    }
    SetProcessDPIAware(); InitializeCriticalSection(&lock);
    stop_event=CreateEvent(NULL,TRUE,FALSE,NULL); refresh_event=CreateEvent(NULL,FALSE,FALSE,NULL);
    wcscpy(model.message,L"Looking for the running local Codex host.");
    fonts[0]=CreateFontW(-12,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Consolas");
    fonts[1]=CreateFontW(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    fonts[2]=CreateFontW(-23,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    fonts[3]=CreateFontW(-36,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    fonts[4]=CreateFontW(-43,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    fonts[5]=CreateFontW(-16,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    if(strstr(cmd,"--probe")) {int result=diagnostics();for(i=0;i<6;i++)DeleteObject(fonts[i]);CloseHandle(stop_event);CloseHandle(refresh_event);DeleteCriticalSection(&lock);return result;}
    memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wndproc; wc.hInstance=instance; wc.lpszClassName=L"AgentGuildNative";
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));if(!wc.hIcon)wc.hIcon=LoadIcon(NULL,IDI_APPLICATION);RegisterClassW(&wc);
    window=CreateWindowExW(0,wc.lpszClassName,L"Agent Guild",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1200,850,NULL,NULL,instance,NULL);
    if(!window)return 10; ShowWindow(window,show); UpdateWindow(window);
    SendMessageW(window,WM_SETICON,ICON_BIG,(LPARAM)wc.hIcon);
    SendMessageW(window,WM_SETICON,ICON_SMALL,(LPARAM)LoadImageW(instance,MAKEINTRESOURCEW(101),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED));
    world_rng=GetTickCount()|1;animation_timer(window);
    {
        HMODULE dwm=LoadLibraryExW(L"dwmapi.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(dwm) {typedef HRESULT (WINAPI *SetAttribute)(HWND,DWORD,LPCVOID,DWORD);FARPROC proc=GetProcAddress(dwm,"DwmSetWindowAttribute");SetAttribute set;BOOL dark=TRUE;memcpy(&set,&proc,sizeof(set));if(set)set(window,20,&dark,sizeof(dark));FreeLibrary(dwm);}
    }
    worker_handle=CreateThread(NULL,0,worker,NULL,0,NULL);
    if(!worker_handle)connection_message(L"Could not start the connection worker.");
    while(GetMessageW(&msg,NULL,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    if(worker_handle) { WaitForSingleObject(worker_handle,6000); CloseHandle(worker_handle); }
    if(terrain_dc){SelectObject(terrain_dc,terrain_old);DeleteObject(terrain_bitmap);DeleteDC(terrain_dc);}
    if(back_dc){SelectObject(back_dc,back_old);DeleteObject(back_bitmap);DeleteDC(back_dc);}
    for(i=0;i<6;i++)DeleteObject(fonts[i]);
    if(instance_mutex)CloseHandle(instance_mutex);CloseHandle(stop_event);CloseHandle(refresh_event);DeleteCriticalSection(&lock);return 0;
}
