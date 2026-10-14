#ifndef GUILD_MODEL_H
#define GUILD_MODEL_H
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "jsmn.h"

#define MAX_AGENTS 512
#define MAX_TOKENS 65536
enum { WORKING, WAITING, IDLE, OFFLINE, FAILED, UNKNOWN };
typedef struct {
    wchar_t id[100], name[240], cwd[520], parent[100], detail[160];
    int state, child;
} Agent;
typedef struct {
    Agent agents[MAX_AGENTS];
    int count, connected, truncated;
    DWORD updated;
    wchar_t message[240];
} Model;
typedef struct { const char *s; jsmntok_t *t; int n; } Json;

static int json_open(Json *j, const char *s) {
    jsmn_parser p;
    j->s=s; j->t=(jsmntok_t*)calloc(MAX_TOKENS,sizeof(jsmntok_t));
    if(!j->t) return 0;
    jsmn_init(&p); j->n=jsmn_parse(&p,s,(unsigned int)strlen(s),j->t,MAX_TOKENS);
    if(j->n<1 || j->t[0].type!=JSMN_OBJECT || j->t[0].end<0 || strspn(s+j->t[0].end," \t\r\n")!=strlen(s+j->t[0].end)) { free(j->t); j->t=NULL; return 0; }
    return 1;
}
static int skip(Json *j,int i) {
    int end;
    if(i<0 || i>=j->n) return j->n;
    end=j->t[i].end; ++i;
    while(i<j->n && j->t[i].start<end) ++i;
    return i;
}
static int equal(Json *j,int i,const char *s) {
    return i>=0 && i<j->n && j->t[i].end-j->t[i].start==(int)strlen(s)
        && !memcmp(j->s+j->t[i].start,s,strlen(s));
}
static int member(Json *j,int obj,const char *key) {
    int i;
    if(obj<0 || obj>=j->n || j->t[obj].type!=JSMN_OBJECT) return -1;
    for(i=obj+1;i<j->n && j->t[i].start<j->t[obj].end;) {
        int value=i+1;
        if(value>=j->n) break;
        if(equal(j,i,key)) return value;
        i=skip(j,value);
    }
    return -1;
}
static int hex4(const char *p) {
    int n=0,k,v;
    for(k=0;k<4;k++) {
        v=p[k]>='0'&&p[k]<='9'?p[k]-'0':p[k]>='a'&&p[k]<='f'?p[k]-'a'+10:p[k]>='A'&&p[k]<='F'?p[k]-'A'+10:-1;
        if(v<0) return -1; n=n*16+v;
    }
    return n;
}
/* Decode JSON escapes and UTF-8, including UTF-16 surrogate pairs. */
static void string_value(Json *j,int i,wchar_t *out,int cap) {
    const char *p,*end; int n=0;
    if(cap<1) return; out[0]=0;
    if(i<0 || i>=j->n || j->t[i].type!=JSMN_STRING) return;
    p=j->s+j->t[i].start; end=j->s+j->t[i].end;
    while(p<end && n<cap-1) {
        unsigned char c=(unsigned char)*p++;
        if(c=='\\' && p<end) {
            c=(unsigned char)*p++;
            if(c=='u' && end-p>=4) {
                int u=hex4(p); p+=4; if(u<0) break;
                if(u>=0xd800 && u<=0xdbff && end-p>=6 && p[0]=='\\' && p[1]=='u') {
                    int low=hex4(p+2);
                    if(low>=0xdc00 && low<=0xdfff && n<cap-2) { out[n++]=(wchar_t)u; out[n++]=(wchar_t)low; p+=6; continue; }
                }
                out[n++]=(wchar_t)(u>=0xd800&&u<=0xdfff?0xfffd:u); continue;
            }
            if(c=='n'||c=='r'||c=='t'||c=='b'||c=='f') c=' ';
            out[n++]=(wchar_t)c;
        } else if(c<0x80) out[n++]=c<32?' ':c;
        else {
            wchar_t w[2]; int bytes=c<0xe0?2:c<0xf0?3:4,got;
            --p; if(end-p<bytes) break;
            got=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,p,bytes,w,2);
            if(got && n+got<cap) { out[n++]=w[0]; if(got==2) out[n++]=w[1]; }
            else if(n<cap-1) out[n++]=0xfffd;
            p+=bytes;
        }
    }
    out[n]=0;
}
static void parse_agent(Json *j,int obj,Agent *a) {
    int st,flags,source,sub,spawn,i; wchar_t type[40],flag[80];
    memset(a,0,sizeof(*a)); a->state=UNKNOWN;
    string_value(j,member(j,obj,"id"),a->id,100);
    string_value(j,member(j,obj,"name"),a->name,240);
    if(!a->name[0]) wcscpy(a->name,L"Untitled quest");
    string_value(j,member(j,obj,"cwd"),a->cwd,520);
    string_value(j,member(j,obj,"parentThreadId"),a->parent,100);
    if(a->parent[0]) a->child=1;
    st=member(j,obj,"status");
    if(st>=0 && j->t[st].type==JSMN_STRING) string_value(j,st,type,40);
    else string_value(j,member(j,st,"type"),type,40);
    if(!wcscmp(type,L"active")) a->state=WORKING;
    else if(!wcscmp(type,L"idle")) a->state=IDLE;
    else if(!wcscmp(type,L"notLoaded")) a->state=OFFLINE;
    else if(!wcscmp(type,L"systemError")) a->state=FAILED;
    flags=member(j,st,"activeFlags");
    if(flags>=0 && j->t[flags].type==JSMN_ARRAY) {
        for(i=flags+1;i<j->n && j->t[i].start<j->t[flags].end;i=skip(j,i)) {
            string_value(j,i,flag,80);
            if(wcsstr(flag,L"waiting") || wcsstr(flag,L"Waiting")) { a->state=WAITING; wcscpy(a->detail,flag); }
        }
    }
    source=member(j,obj,"source");
    if(source>=0 && j->t[source].type==JSMN_OBJECT) {
        sub=member(j,source,"subAgent");
        if(sub>=0) {
            a->child=1; spawn=member(j,sub,"thread_spawn");
            if(spawn<0) spawn=member(j,sub,"threadSpawn");
            if(!a->parent[0]) string_value(j,member(j,spawn,"parent_thread_id"),a->parent,100);
            if(!a->parent[0]) string_value(j,member(j,spawn,"parentThreadId"),a->parent,100);
            if(!wcscmp(a->name,L"Untitled quest")) {
                wchar_t path[240];
                string_value(j,member(j,spawn,"agent_path"),path,240);
                if(path[0]) {wchar_t *name=wcsrchr(path,L'/');wcscpy(a->name,name?name+1:path);}
            }
        }
    }
    if(!wcscmp(a->name,L"Untitled quest")) {
        wchar_t nickname[240];string_value(j,member(j,obj,"agentNickname"),nickname,240);
        if(nickname[0])wcscpy(a->name,nickname);
    }
}
static int agent_cmp(const void *pa,const void *pb) {
    const Agent *a=(const Agent*)pa,*b=(const Agent*)pb;
    if(a->state!=b->state) return a->state-b->state;
    return _wcsicmp(a->name,b->name);
}
#endif
