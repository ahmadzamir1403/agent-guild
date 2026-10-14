#ifndef GUILD_ECONOMY_H
#define GUILD_ECONOMY_H
#define MAX_PROGRESS 1024
typedef struct {wchar_t id[100];int gold,power,boots,luck,spent,kills,last_purchase_kills;} Progress;
static Progress progress[MAX_PROGRESS];
static int progress_count,progress_dirty;
static DWORD progress_saved;
static wchar_t progress_path[520];
static const wchar_t *upgrade_names[]={L"Power",L"Boots",L"Luck"};
static const wchar_t *buff_names[]={L"",L"FURY",L"HASTE",L"FORTUNE"};
static int gold_drop(Hero *h,int roll,int bonus_roll) {
    int reward=roll<25?0:roll<80?1:roll<96?2:3;
    if(bonus_roll<h->luck_level*12)reward++;
    if(h->buff_kind==3&&h->buff_time>0)reward*=2;
    return reward;
}
static int upgrade_level(Hero *h,int choice){return choice==0?h->power_level:choice==1?h->boots_level:h->luck_level;}
static int upgrade_cost(Hero *h,int choice){static const int costs[]={20,45,85,140,140};return costs[upgrade_level(h,choice)];}
static int gold_reserve(Hero *h){return 4+((h->seed/3)%3)*6+2*(h->power_level+h->boots_level+h->luck_level);}
static int choose_upgrade(Hero *h,int affordable) {
    int i,best=-1;float score,best_score=-1;
    static const int priorities[3][3]={{5,3,2},{4,2,5},{3,5,2}};
    for(i=0;i<3;i++) {
        int level=upgrade_level(h,i),cost=upgrade_cost(h,i);
        if(level>=4||(affordable&&h->gold<cost))continue;
        score=(float)priorities[h->seed%3][i]/((level+1)*cost);
        if(score>best_score){best_score=score;best=i;}
    }
    return best;
}
static int shop_decision(Hero *h) {
    int choice;
    if(h->shopping_cooldown>0||h->kills-h->last_shop_review<3+(int)(h->seed%3)||h->kills-h->last_purchase_kills<4)return -1;
    h->last_shop_review=h->kills;choice=choose_upgrade(h,0);
    if(choice<0||h->gold<upgrade_cost(h,choice)+gold_reserve(h))return -1;
    /* Personal goals and a reserve come before random shopping opportunities. */
    if(random_next()%100>=35+(h->seed%3)*10)return -1;
    return choice;
}
static void economy_reset(void){memset(progress,0,sizeof(progress));progress_count=progress_dirty=0;}
static Progress *economy_entry(const wchar_t *id,int create) {
    int i;for(i=0;i<progress_count;i++)if(!wcscmp(progress[i].id,id))return &progress[i];
    if(!create||progress_count>=MAX_PROGRESS)return NULL;
    wcscpy(progress[progress_count].id,id);return &progress[progress_count++];
}
static void economy_apply(Hero *h) {
    Progress *p=economy_entry(h->id,0);if(!p)return;
    h->gold=p->gold;h->power_level=p->power;h->boots_level=p->boots;h->luck_level=p->luck;h->spent=p->spent;h->kills=p->kills;h->last_purchase_kills=p->last_purchase_kills;h->last_shop_review=p->kills;
}
static void economy_record(Hero *h) {
    Progress *p=economy_entry(h->id,1);if(!p)return;
    p->gold=h->gold;p->power=h->power_level;p->boots=h->boots_level;p->luck=h->luck_level;p->spent=h->spent;p->kills=h->kills;p->last_purchase_kills=h->last_purchase_kills;progress_dirty=1;
}
static int economy_buy(Hero *h,int choice) {
    int cost;
    if(choice<0||choice>2||upgrade_level(h,choice)>=4)return 0;
    cost=upgrade_cost(h,choice);if(h->gold<cost)return 0;
    h->gold-=cost;h->spent+=cost;h->last_price=cost;
    if(choice==0)h->power_level++;else if(choice==1)h->boots_level++;else h->luck_level++;
    h->shop_choice=choice;h->purchase_fx=2.3f;h->last_purchase_kills=h->kills;h->shopping_cooldown=45+(h->seed%46);economy_record(h);return 1;
}
static int bounded_value(Json *j,int obj,const char *key,int maximum) {
    int token=member(j,obj,key);long value;
    if(token<0||j->t[token].type!=JSMN_PRIMITIVE)return 0;
    value=strtol(j->s+j->t[token].start,NULL,10);if(value<0)return 0;if(value>maximum)return maximum;return (int)value;
}
static int economy_parse(const char *raw) {
    Json j;int arr,i;Progress *p;wchar_t id[100];
    if(!json_open(&j,raw))return 0;
    if(bounded_value(&j,0,"version",100)!=1){free(j.t);return 0;}
    arr=member(&j,0,"heroes");if(arr<0||j.t[arr].type!=JSMN_ARRAY){free(j.t);return 0;}
    economy_reset();
    for(i=arr+1;i<j.n&&j.t[i].start<j.t[arr].end;i=skip(&j,i)) {
        if(j.t[i].type!=JSMN_OBJECT)continue;string_value(&j,member(&j,i,"id"),id,100);if(!id[0])continue;
        p=economy_entry(id,1);if(!p)break;
        p->gold=bounded_value(&j,i,"gold",1000000);p->power=bounded_value(&j,i,"power",4);p->boots=bounded_value(&j,i,"boots",4);p->luck=bounded_value(&j,i,"luck",4);
        p->spent=bounded_value(&j,i,"spent",1000000);p->kills=bounded_value(&j,i,"kills",1000000);
        p->last_purchase_kills=bounded_value(&j,i,"lastPurchaseKills",1000000);
    }
    free(j.t);progress_dirty=0;return 1;
}
static void economy_read(const wchar_t *path) {
    HANDLE file;DWORD length,read;char *raw;
    file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(file==INVALID_HANDLE_VALUE)return;
    length=GetFileSize(file,NULL);if(length>1024*1024){CloseHandle(file);return;}
    raw=(char*)malloc(length+1);if(!raw){CloseHandle(file);return;}
    if(ReadFile(file,raw,length,&read,NULL)&&read==length){raw[length]=0;economy_parse(raw);}free(raw);CloseHandle(file);
}
static void economy_load(void) {
    wchar_t base[400];DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",base,400);if(!n||n>=400)return;
    _snwprintf(progress_path,519,L"%ls\\AgentGuild",base);progress_path[519]=0;CreateDirectoryW(progress_path,NULL);wcscat(progress_path,L"\\progress.json");economy_read(progress_path);
}
static int write_text(HANDLE file,const char *text) {DWORD n,wanted=(DWORD)strlen(text);return WriteFile(file,text,wanted,&n,NULL)&&n==wanted;}
static int economy_save(int force) {
    wchar_t temp[530];char row[1200],id[800];HANDLE file;int i,ok=1;
    if(!progress_dirty||!progress_path[0]||(!force&&GetTickCount()-progress_saved<15000))return 1;
    progress_saved=GetTickCount();
    _snwprintf(temp,529,L"%ls.tmp",progress_path);temp[529]=0;
    file=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(file==INVALID_HANDLE_VALUE)return 0;
    ok=write_text(file,"{\"version\":1,\"heroes\":[\n");
    for(i=0;i<progress_count&&ok;i++) {
        Progress *p=&progress[i];if(!quote_id(p->id,id,sizeof(id))){ok=0;break;}
        snprintf(row,sizeof(row),"%s{\"id\":%s,\"gold\":%d,\"power\":%d,\"boots\":%d,\"luck\":%d,\"spent\":%d,\"kills\":%d,\"lastPurchaseKills\":%d}\n",i?",":"",id,p->gold,p->power,p->boots,p->luck,p->spent,p->kills,p->last_purchase_kills);
        ok=write_text(file,row);
    }
    if(ok)ok=write_text(file,"]}\n");if(ok)ok=FlushFileBuffers(file);CloseHandle(file);
    if(ok)ok=MoveFileExW(temp,progress_path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
    if(ok){progress_dirty=0;progress_saved=GetTickCount();}else DeleteFileW(temp);
    return ok;
}
static int economy_test(void) {
    Hero h,restored;int initial,choice,i;
    memset(&h,0,sizeof(h));wcscpy(h.id,L"economy-test");h.seed=0;h.gold=20;initial=h.gold;
    choice=choose_upgrade(&h,1);if(choice!=0||!economy_buy(&h,choice)||h.gold!=initial-20||h.power_level!=1)return 61;
    if(economy_buy(&h,0)||h.gold<0||h.power_level!=1)return 62;
    memset(&restored,0,sizeof(restored));wcscpy(restored.id,h.id);economy_apply(&restored);if(restored.power_level!=1||restored.spent!=20)return 63;
    h.gold=10000;for(i=0;i<30;i++){choice=choose_upgrade(&h,1);if(choice>=0)economy_buy(&h,choice);}
    if(h.power_level!=4||h.boots_level!=4||h.luck_level!=4||choose_upgrade(&h,1)!=-1)return 64;
    if(!economy_parse("{\"version\":1,\"heroes\":[{\"id\":\"bounded\",\"gold\":999999999,\"power\":100,\"boots\":-1,\"luck\":2}]}"))return 65;
    memset(&restored,0,sizeof(restored));wcscpy(restored.id,L"bounded");economy_apply(&restored);if(restored.gold!=1000000||restored.power_level!=4||restored.boots_level!=0||restored.luck_level!=2)return 66;
    memset(&h,0,sizeof(h));h.seed=0;h.gold=20;h.kills=10;if(shop_decision(&h)!=-1)return 71;
    h.gold=100;h.kills=11;h.last_shop_review=10;if(shop_decision(&h)!=-1)return 72;
    h.shopping_cooldown=1;h.kills=30;if(shop_decision(&h)!=-1)return 73;
    {int buys=0,defers=0;h.shopping_cooldown=0;world_rng=12345;for(i=0;i<30;i++){h.kills+=5;if(shop_decision(&h)>=0)buys++;else defers++;}if(!buys||!defers)return 74;}
    {int sum=0,empty=0;memset(&h,0,sizeof(h));for(i=0;i<100;i++){int coins=gold_drop(&h,i,99);sum+=coins;if(!coins)empty++;}if(sum!=99||empty!=25)return 75;h.luck_level=4;if(gold_drop(&h,25,0)!=2||gold_drop(&h,25,99)!=1)return 76;h.buff_kind=3;h.buff_time=1;if(gold_drop(&h,25,99)!=2)return 77;}
    economy_reset();return 0;
}
static int economy_file_test(void) {
    wchar_t temp[400];DWORD n=GetTempPathW(400,temp);Hero h,loaded;int result=0;
    if(!n||n>=400)return 67;
    _snwprintf(progress_path,519,L"%lsAgentGuild-test-%lu-%lu.json",temp,(unsigned long)GetCurrentProcessId(),(unsigned long)GetTickCount());progress_path[519]=0;
    memset(&h,0,sizeof(h));wcscpy(h.id,L"durability-test");h.gold=123;h.power_level=2;h.boots_level=1;h.luck_level=3;h.spent=81;h.kills=17;economy_record(&h);
    if(!economy_save(1))result=68;
    else {economy_reset();economy_read(progress_path);memset(&loaded,0,sizeof(loaded));wcscpy(loaded.id,h.id);economy_apply(&loaded);if(loaded.gold!=123||loaded.power_level!=2||loaded.boots_level!=1||loaded.luck_level!=3||loaded.spent!=81||loaded.kills!=17)result=69;}
    DeleteFileW(progress_path);progress_path[0]=0;economy_reset();return result;
}
#endif
