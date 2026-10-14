/* Passive, local-only Codex app-server client. No agent turns or mutations. */
#ifndef GUILD_TRANSPORT_H
#define GUILD_TRANSPORT_H
#define MAX_MESSAGE (2*1024*1024)
static HANDLE refresh_event;
static HCRYPTPROV crypto;
static SOCKET connection=INVALID_SOCKET;

static int stopping(void) { return WaitForSingleObject(stop_event,0)==WAIT_OBJECT_0; }
static int ready(SOCKET s,int writing,DWORD deadline) {
    while(!stopping() && (LONG)(deadline-GetTickCount())>0) {
        fd_set set; struct timeval tv; int result;
        FD_ZERO(&set); FD_SET(s,&set); tv.tv_sec=0;tv.tv_usec=100000;
        result=select(0,writing?NULL:&set,writing?&set:NULL,NULL,&tv);
        if(result>0)return 1;if(result==SOCKET_ERROR)return 0;
    }
    return 0;
}
static int receive_exact(SOCKET s,void *data,int size,DWORD deadline) {
    char *p=(char*)data;int got;
    while(size>0) {
        if(!ready(s,0,deadline))return 0;
        got=recv(s,p,size,0);if(got<=0)return 0;p+=got;size-=got;
    }
    return 1;
}
static int send_exact(SOCKET s,const void *data,int size) {
    const char *p=(const char*)data; int sent;DWORD deadline=GetTickCount()+5000;
    while(size>0) {
        if(!ready(s,1,deadline))return 0;
        sent=send(s,p,size,0);if(sent<=0)return 0;p+=sent;size-=sent;
    }
    return 1;
}
static void base64(const unsigned char *data,int n,char *out) {
    static const char abc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i,k=0;unsigned int v;
    for(i=0;i<n;i+=3) {
        v=(unsigned int)data[i]<<16;if(i+1<n)v|=(unsigned int)data[i+1]<<8;if(i+2<n)v|=data[i+2];
        out[k++]=abc[(v>>18)&63];out[k++]=abc[(v>>12)&63];out[k++]=i+1<n?abc[(v>>6)&63]:'=';out[k++]=i+2<n?abc[v&63]:'=';
    }
    out[k]=0;
}
static int upgrade(SOCKET s) {
    unsigned char random[16],hash[20]; char key[32],expected[32],challenge[100],request[512],response[8192],lower[8192];
    DWORD hash_size=20,deadline=GetTickCount()+5000; HCRYPTHASH sha;int n=0,i;char *accept;
    if(!CryptGenRandom(crypto,16,random))return 0;base64(random,16,key);
    snprintf(challenge,sizeof(challenge),"%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11",key);
    if(!CryptCreateHash(crypto,CALG_SHA1,0,0,&sha))return 0;
    if(!CryptHashData(sha,(BYTE*)challenge,(DWORD)strlen(challenge),0)||!CryptGetHashParam(sha,HP_HASHVAL,hash,&hash_size,0)) {CryptDestroyHash(sha);return 0;}
    CryptDestroyHash(sha);base64(hash,20,expected);
    snprintf(request,sizeof(request),"GET / HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n",key);
    if(!send_exact(s,request,(int)strlen(request)))return 0;
    while(n<(int)sizeof(response)-1) {
        if(!receive_exact(s,response+n,1,deadline))return 0;n++;response[n]=0;
        if(n>=4 && !memcmp(response+n-4,"\r\n\r\n",4))break;
    }
    if(n>=(int)sizeof(response)-1 || strncmp(response,"HTTP/1.1 101 ",13))return 0;
    for(i=0;i<=n;i++)lower[i]=response[i]>='A'&&response[i]<='Z'?response[i]+32:response[i];
    if(!strstr(lower,"\r\nupgrade: websocket\r\n"))return 0;
    accept=strstr(lower,"\r\nsec-websocket-accept:");if(!accept)return 0;
    accept=response+(accept-lower)+23;while(*accept==' '||*accept=='\t')accept++;
    return !strncmp(accept,expected,strlen(expected)) && accept[strlen(expected)]=='\r';
}
static int send_frame(SOCKET s,int opcode,const char *data,int size) {
    unsigned char header[14],mask[4]; char *body;int n=0,i,ok;
    if(size<0||size>MAX_MESSAGE||!CryptGenRandom(crypto,4,mask))return 0;
    header[n++]=(unsigned char)(0x80|opcode);
    if(size<126)header[n++]=(unsigned char)(0x80|size);
    else if(size<65536) {header[n++]=0xfe;header[n++]=(unsigned char)(size>>8);header[n++]=(unsigned char)size;}
    else {header[n++]=0xff;header[n++]=0;header[n++]=0;header[n++]=0;header[n++]=0;for(i=3;i>=0;i--)header[n++]=(unsigned char)((unsigned int)size>>(8*i));}
    memcpy(header+n,mask,4);n+=4;
    body=(char*)malloc(size?size:1);if(!body)return 0;
    for(i=0;i<size;i++)body[i]=(char)(data[i]^mask[i%4]);
    ok=send_exact(s,header,n)&&send_exact(s,body,size);free(body);return ok;
}
static char *read_message(SOCKET s,DWORD deadline) {
    char *out=(char*)malloc(MAX_MESSAGE+1);int used=0,fragmented=0;
    if(!out)return NULL;
    while(!stopping()) {
        unsigned char h[2],ext[8];unsigned long long size;int op,fin,i;
        if(!receive_exact(s,h,2,deadline))break;
        fin=h[0]&128;op=h[0]&15;size=h[1]&127;
        if((h[0]&0x70)||(h[1]&128))break; /* No extensions or masked server frames. */
        if(size==126) {if(!receive_exact(s,ext,2,deadline))break;size=((unsigned int)ext[0]<<8)|ext[1];}
        else if(size==127) {if(!receive_exact(s,ext,8,deadline))break;size=0;for(i=0;i<8;i++)size=(size<<8)|ext[i];}
        if(op>=8) {
            char control[125];
            if(!fin||size>125||!receive_exact(s,control,(int)size,deadline))break;
            if(op==8)break;
            if(op==9) {if(!send_frame(s,10,control,(int)size))break;continue;}
            if(op==10)continue;
            break;
        }
        if((op!=1&&op!=0)||(op==0&&!fragmented)||(op==1&&fragmented)||size>(unsigned int)(MAX_MESSAGE-used))break;
        if(!receive_exact(s,out+used,(int)size,deadline))break;used+=(int)size;
        if(fin) {out[used]=0;return out;}
        fragmented=1;
    }
    free(out);return NULL;
}
static char *rpc(SOCKET s,const char *request,int id) {
    DWORD deadline=GetTickCount()+10000;
    if(!send_frame(s,1,request,(int)strlen(request)))return NULL;
    while(!stopping() && (LONG)(deadline-GetTickCount())>0) {
        char *raw=read_message(s,deadline);Json j;int token,matched;
        if(!raw)return NULL;
        if(!json_open(&j,raw)){free(raw);return NULL;}
        token=member(&j,0,"id");matched=token>=0 && j.t[token].type==JSMN_PRIMITIVE && atoi(raw+j.t[token].start)==id;
        if(matched) {
            int error=member(&j,0,"error"); free(j.t);
            if(error>=0){free(raw);return NULL;}return raw;
        }
        /* Other notifications and server requests never trigger approval responses. */
        free(j.t);free(raw);
    }
    return NULL;
}
static int quote_id(const wchar_t *id,char *out,int capacity) {
    char utf8[400];int length=WideCharToMultiByte(CP_UTF8,0,id,-1,utf8,400,NULL,NULL),i,n=0;
    if(!length)return 0;out[n++]='"';
    for(i=0;i<length-1;i++) {
        unsigned char c=utf8[i];if(c<32||n+3>=capacity)return 0;
        if(c=='"'||c=='\\')out[n++]='\\';out[n++]=c;
    }
    out[n++]='"';out[n]=0;return 1;
}
static int snapshot(SOCKET s,Model *next,int *request_id) {
    char request[1024],quoted[800],*raw;Json j;int arr,i,n=0,r,id;
    wchar_t (*ids)[100]=(wchar_t(*)[100])calloc(MAX_AGENTS,100*sizeof(wchar_t));
    if(!ids)return 0;memset(next,0,sizeof(*next));
    id=++*request_id;snprintf(request,sizeof(request),"{\"id\":%d,\"method\":\"thread/loaded/list\",\"params\":{}}",id);
    raw=rpc(s,request,id);if(!raw){free(ids);return 0;}
    if(!json_open(&j,raw)){free(ids);free(raw);return 0;}
    arr=member(&j,member(&j,0,"result"),"data");
    if(arr<0||j.t[arr].type!=JSMN_ARRAY){free(j.t);free(ids);free(raw);return 0;}
    for(i=arr+1;i<j.n && j.t[i].start<j.t[arr].end;i=skip(&j,i)) {
        if(n>=MAX_AGENTS){next->truncated=1;break;}
        if(j.t[i].type!=JSMN_STRING){free(j.t);free(ids);free(raw);return 0;}
        string_value(&j,i,ids[n],100);n++;
    }
    free(j.t);free(raw);
    for(i=0;i<n && !stopping();i++) {
        if(!quote_id(ids[i],quoted,sizeof(quoted))){free(ids);return 0;}
        id=++*request_id;
        snprintf(request,sizeof(request),"{\"id\":%d,\"method\":\"thread/read\",\"params\":{\"threadId\":%s,\"includeTurns\":false}}",id,quoted);
        raw=rpc(s,request,id);if(!raw){free(ids);return 0;}
        if(!json_open(&j,raw)){free(ids);free(raw);return 0;}
        r=member(&j,member(&j,0,"result"),"thread");
        if(r<0||j.t[r].type!=JSMN_OBJECT){free(j.t);free(ids);free(raw);return 0;}
        parse_agent(&j,r,&next->agents[next->count]);
        if(next->agents[next->count].id[0] && next->agents[next->count].state!=OFFLINE)next->count++;
        free(j.t);free(raw);
    }
    free(ids);if(stopping())return 0;
    qsort(next->agents,next->count,sizeof(Agent),agent_cmp);
    next->connected=1;next->updated=GetTickCount();
    wcscpy(next->message,L"Connected to the local Codex daemon.");return 1;
}
static SOCKET connect_host(void) {
    struct {unsigned short family;char path[108];} address;
    wchar_t path[520],home[400];SOCKET s;u_long nonblocking=1;int result,len;
    {
        len=GetEnvironmentVariableW(L"CODEX_HOME",home,400);
        if(len>=400)return INVALID_SOCKET;
        if(!len) {len=GetEnvironmentVariableW(L"USERPROFILE",home,360);if(!len||len>=360)return INVALID_SOCKET;wcscat(home,L"\\.codex");}
        _snwprintf(path,519,L"%ls\\app-server-control\\app-server-control.sock",home);path[519]=0;
    }
    memset(&address,0,sizeof(address));address.family=1;
    if(!WideCharToMultiByte(CP_UTF8,0,path,-1,address.path,108,NULL,NULL))return INVALID_SOCKET;
    s=socket(1,SOCK_STREAM,0);if(s==INVALID_SOCKET)return s;
    ioctlsocket(s,FIONBIO,&nonblocking);
    result=connect(s,(struct sockaddr*)&address,sizeof(address));
    if(result==SOCKET_ERROR) {
        int error=WSAGetLastError();
        if(error!=WSAEWOULDBLOCK || !ready(s,1,GetTickCount()+3000)) {closesocket(s);return INVALID_SOCKET;}
        len=sizeof(error);if(getsockopt(s,SOL_SOCKET,SO_ERROR,(char*)&error,&len)||error) {closesocket(s);return INVALID_SOCKET;}
    }
    return s;
}
static DWORD WINAPI worker(LPVOID unused) {
    WSADATA wsa; Model *next=(Model*)calloc(1,sizeof(Model));int request_id=1;
    HANDLE waits[2];waits[0]=stop_event;waits[1]=refresh_event;
    if(!next || WSAStartup(MAKEWORD(2,2),&wsa) || !CryptAcquireContext(&crypto,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT)) {
        free(next);connection_message(L"Windows networking could not initialize.");return 1;
    }
    while(!stopping()) {
        char *raw;
        connection=connect_host();
        if(connection==INVALID_SOCKET){connection_message(L"Open Codex to bring your party online. Retrying...");WaitForMultipleObjects(2,waits,FALSE,4000);continue;}
        if(!upgrade(connection)) {connection_message(L"Codex handshake failed. Retrying...");closesocket(connection);connection=INVALID_SOCKET;WaitForMultipleObjects(2,waits,FALSE,4000);continue;}
        request_id=1;
        raw=rpc(connection,"{\"id\":1,\"method\":\"initialize\",\"params\":{\"clientInfo\":{\"name\":\"agent_guild\",\"title\":\"Agent Guild\",\"version\":\"0.3.0\"}}}",1);
        if(raw) {
            free(raw);
            if(send_frame(connection,1,"{\"method\":\"initialized\"}",24)) {
                while(!stopping() && snapshot(connection,next,&request_id)) {
                    publish(next);
                    if(probe_mode){SetEvent(stop_event);break;}
                    if(WaitForMultipleObjects(2,waits,FALSE,IsIconic(window)?10000:2000)==WAIT_OBJECT_0)break;
                }
            }
        }
        closesocket(connection);connection=INVALID_SOCKET;
        if(!stopping()){connection_message(L"Connection interrupted. Last states may be stale.");WaitForMultipleObjects(2,waits,FALSE,3000);}
    }
    free(next);CryptReleaseContext(crypto,0);WSACleanup();return 0;
}
#endif
