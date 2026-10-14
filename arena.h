#ifndef GUILD_ARENA_H
#define GUILD_ARENA_H
#define MAP_X 28
#define MAP_Y 155
#define MAP_W 810
#define MAP_H 520
static HDC terrain_dc;
static HBITMAP terrain_bitmap,terrain_old;
static RECT hero_hits[MAX_HEROES];
static int hero_slots[MAX_HEROES],hero_hit_count;
static int world_view=1,paused;
static RECT view_hits[2],pause_hit;
static DWORD last_frame;

static void ellipse_fill(HDC dc,int x,int y,int w,int h,COLORREF c) {
    HGDIOBJ b=SelectObject(dc,GetStockObject(DC_BRUSH)),p=SelectObject(dc,GetStockObject(NULL_PEN));
    SetDCBrushColor(dc,c);Ellipse(dc,x,y,x+w,y+h);SelectObject(dc,b);SelectObject(dc,p);
}
static void tree(HDC dc,int x,int y,int scale) {
    int p=scale;
    fill(dc,x-9*p,y+23*p,22*p,5*p,RGB(19,37,33));
    fill(dc,x,y+13*p,4*p,13*p,RGB(89,72,53));
    fill(dc,x-7*p,y+9*p,18*p,11*p,RGB(27,65,48));
    fill(dc,x-5*p,y+3*p,14*p,12*p,RGB(37,83,57));
    fill(dc,x-2*p,y,8*p,10*p,RGB(47,97,64));
    fill(dc,x-4*p,y+6*p,3*p,4*p,RGB(62,113,72));
}
static void terrain_create(HDC dc) {
    int x,y,i;unsigned int v;
    if(terrain_dc)return;
    terrain_dc=CreateCompatibleDC(dc);terrain_bitmap=CreateCompatibleBitmap(dc,MAP_W,MAP_H);
    terrain_old=SelectObject(terrain_dc,terrain_bitmap);
    fill(terrain_dc,0,0,MAP_W,MAP_H,RGB(28,49,42));
    for(y=0;y<MAP_H;y+=20)for(x=0;x<MAP_W;x+=20) {
        v=(unsigned int)(x*9781+y*6271+173);v^=v>>11;v*=2654435761u;
        if(v%4==0)fill(terrain_dc,x,y,20,20,RGB(30,52,44));
        if(v%7==0) {fill(terrain_dc,x+4,y+8,2,5,RGB(49,73,49));fill(terrain_dc,x+7,y+10,2,3,RGB(49,73,49));}
        if(v%61==0){fill(terrain_dc,x+9,y+7,3,3,RGB(154,150,105));fill(terrain_dc,x+7,y+9,7,2,RGB(107,111,72));}
    }
    /* An old path joins the camp, grove, ruins and shore. */
    fill(terrain_dc,103,350,288,34,RGB(57,65,48));fill(terrain_dc,346,112,34,258,RGB(57,65,48));
    fill(terrain_dc,180,124,454,30,RGB(57,65,48));fill(terrain_dc,580,139,30,236,RGB(57,65,48));
    fill(terrain_dc,367,337,239,32,RGB(57,65,48));fill(terrain_dc,103,354,29,100,RGB(57,65,48));
    for(i=0;i<35;i++) {x=110+(i*73)%490;y=350+(i*11)%20;fill(terrain_dc,x,y,4,2,RGB(79,79,56));}
    /* Stepped shoreline, stones, and ruined columns. */
    fill(terrain_dc,556,419,200,58,RGB(37,74,76));fill(terrain_dc,578,403,148,92,RGB(35,83,87));
    fill(terrain_dc,608,394,89,111,RGB(36,88,92));
    for(i=0;i<20;i++)fill(terrain_dc,575+(i*37)%152,416+(i*17)%68,13+(i%3)*5,2,RGB(53,110,108));
    for(i=0;i<4;i++) {x=410+(i%2)*27;y=265+(i/2)*25;fill(terrain_dc,x,y,15,22,RGB(99,109,98));fill(terrain_dc,x+3,y+3,9,8,RGB(131,137,119));fill(terrain_dc,x-2,y+19,19,4,RGB(71,83,75));}
    for(i=0;i<16;i++){x=260+(i*157)%430;y=173+(i*61)%219;if(x>395&&x<465)continue;fill(terrain_dc,x,y,10,6,RGB(66,83,71));fill(terrain_dc,x+2,y-2,6,3,RGB(91,106,86));}
    /* Outer forest leaves plenty of clear walking space. */
    for(x=8;x<MAP_W;x+=39){tree(terrain_dc,x,1,2);if(x<520||x>750)tree(terrain_dc,x,464,2);}
    for(y=60;y<440;y+=58){tree(terrain_dc,4,y,2);tree(terrain_dc,778,y,2);}
    tree(terrain_dc,270,199,2);tree(terrain_dc,489,52,2);tree(terrain_dc,703,239,2);tree(terrain_dc,80,191,2);
    /* Camp tent, bedrolls, and help lantern. */
    for(i=0;i<12;i++)fill(terrain_dc,70-i*3,322+i*3,6+i*6,3,RGB(128,112,79));
    fill(terrain_dc,57,343,26,15,RGB(44,49,42));fill(terrain_dc,65,339,10,19,RGB(28,37,33));
    fill(terrain_dc,94,345,22,8,RGB(97,92,71));fill(terrain_dc,95,344,20,2,RGB(144,125,86));
    fill(terrain_dc,215,369,4,42,RGB(112,92,58));fill(terrain_dc,208,368,18,5,RGB(112,92,58));
    fill(terrain_dc,209,374,12,16,RGB(91,107,93));fill(terrain_dc,212,377,6,10,RGB(239,188,90));
    fill(terrain_dc,113,425,28,5,RGB(107,78,51));fill(terrain_dc,121,418,6,17,RGB(130,90,51));
    fill(terrain_dc,262,421,53,31,RGB(105,78,53));fill(terrain_dc,267,426,43,20,RGB(68,58,43));
    for(i=0;i<6;i++)fill(terrain_dc,258+i*10,410,10,11,i%2?RGB(210,185,130):RGB(153,103,74));
    avatar(terrain_dc,277,427,24,1,RGB(225,182,103));fill(terrain_dc,266,450,45,5,RGB(140,110,68));
    label(terrain_dc,247,462,100,20,L"UPGRADES",0,RGB(213,184,116),DT_CENTER|DT_SINGLELINE);
    label(terrain_dc,50,294,190,20,L"THE CAMP",0,RGB(181,176,131),DT_SINGLELINE);
    label(terrain_dc,98,73,190,20,L"MOSSFALL GROVE",0,RGB(127,155,116),DT_SINGLELINE);
    label(terrain_dc,307,52,200,20,L"EMBERROOT HOLLOW",0,RGB(172,151,107),DT_SINGLELINE);
    label(terrain_dc,562,72,210,20,L"MOONFERN GLADE",0,RGB(121,165,160),DT_SINGLELINE);
    label(terrain_dc,331,237,190,20,L"OLDSTONE RUINS",0,RGB(146,157,135),DT_SINGLELINE);
    label(terrain_dc,547,382,210,20,L"SILVERLEAF SHORE",0,RGB(128,172,169),DT_SINGLELINE);
}
static void monster_draw(HDC dc,Hero *h) {
    int x=MAP_X+(int)h->mx,y=MAP_Y+(int)h->my,bob=(int)(sinf(world_time*3+(h->seed%10))*2),kind=h->monster_kind;
    COLORREF color=h->impact>.12f?RGB(243,239,179):kind==0?RGB(113,173,91):kind==1?RGB(168,132,89):RGB(186,140,175);
    if(h->loot_fx>0) {
        wchar_t loot[40];int lx=MAP_X+(int)h->loot_x,ly=MAP_Y+(int)h->loot_y-(int)((1.8f-h->loot_fx)*17);
        if(h->last_reward)_snwprintf(loot,39,L"+%d GOLD",h->last_reward);else wcscpy(loot,L"NO GOLD");label(dc,lx-45,ly-47,95,22,loot,0,h->last_reward?GOLD:MUTED,DT_CENTER|DT_SINGLELINE);
        if(h->buff_time>10.2f){fill(dc,lx+24,ly-20,17,17,h->buff_kind==1?RGB(224,126,111):h->buff_kind==2?RGB(124,203,230):GOLD);label(dc,lx+24,ly-20,17,17,h->buff_kind==1?L"F":h->buff_kind==2?L"H":L"$",0,BG,DT_CENTER|DT_SINGLELINE);}
    }
    if(h->hp<=0) {
        int i,r=(int)((1-h->respawn)*22);
        if(h->respawn>0)for(i=0;i<8;i++)fill(dc,x+(int)(cosf(i*.785f)*r),y-10+(int)(sinf(i*.785f)*r),3,3,i%2?GOLD:MINT);
        return;
    }
    ellipse_fill(dc,x-14,y+9,30,8,RGB(19,34,31));y+=bob;
    if(kind==0) {
        fill(dc,x-12,y-6,26,15,color);fill(dc,x-8,y-12,18,7,color);fill(dc,x-15,y+1,31,9,color);
        fill(dc,x-9,y-7,5,4,RGB(154,200,115));fill(dc,x-5,y-3,3,4,BG);fill(dc,x+6,y-3,3,4,BG);
    } else if(kind==1) {
        fill(dc,x-9,y-12,21,20,color);fill(dc,x-13,y-16,5,9,RGB(199,179,125));fill(dc,x+10,y-16,5,9,RGB(199,179,125));
        fill(dc,x-7,y+8,5,7,color);fill(dc,x+6,y+8,5,7,color);fill(dc,x-3,y-7,4,4,BG);fill(dc,x+6,y-7,4,4,BG);
        fill(dc,x+14,y-3,3,19,RGB(108,81,52));fill(dc,x+12,y-6,7,7,RGB(133,146,115));
    } else {
        fill(dc,x-9,y-9,21,21,color);fill(dc,x-13,y-11,29,8,RGB(105,80,111));fill(dc,x-4,y-16,12,5,RGB(151,111,153));
        fill(dc,x-7,y+12,6,5,RGB(104,80,104));fill(dc,x+5,y+12,6,5,RGB(104,80,104));fill(dc,x-4,y-2,4,4,BG);fill(dc,x+5,y-2,4,4,BG);
    }
    fill(dc,x-16,y-24,34,5,RGB(15,30,27));fill(dc,x-15,y-23,32*h->hp/h->max_hp,3,RGB(221,141,119));
    if(h->impact>0) {wchar_t damage[16];int r=(int)((.32f-h->impact)*45);fill(dc,x-19-r,y-4-r,4,4,GOLD);fill(dc,x+16+r,y-12-r,4,4,INK);_snwprintf(damage,15,L"-%d",h->last_damage);label(dc,x-12,y-41-r,35,18,damage,0,GOLD,DT_CENTER|DT_SINGLELINE);}
}
static COLORREF hero_color(Hero *h) {
    static const COLORREF palette[]={RGB(142,221,182),RGB(180,165,244),RGB(120,186,230),RGB(241,181,113),RGB(219,145,159),RGB(185,208,117)};
    return model.connected?palette[h->seed%6]:MUTED;
}
static void hero_draw(HDC dc,Hero *h) {
    int x=MAP_X+(int)h->x,y=MAP_Y+(int)h->y,bob=0,walking=fabsf(h->tx-h->x)+fabsf(h->ty-h->y)>3;
    int kind=h->seed%3,chosen=!wcscmp(h->id,selected_id);COLORREF c=hero_color(h);wchar_t text[100];int i;
    if(walking)bob=(int)(sinf(h->step)*2);
    if(h->buff_time>0)ellipse_fill(dc,x-19,y-22,42,40,h->buff_kind==1?RGB(71,44,35):h->buff_kind==2?RGB(34,65,68):RGB(71,63,33));
    ellipse_fill(dc,x-13,y+8,29,9,RGB(18,32,30));
    if(walking&&h->buff_kind==2&&h->buff_time>0){fill(dc,x-24,y-4,6,3,RGB(71,136,148));fill(dc,x-33,y-7,5,3,RGB(51,102,112));}
    if(chosen) {fill(dc,x-17,y+13,38,2,MINT);fill(dc,x-17,y+6,2,9,MINT);fill(dc,x+19,y+6,2,9,MINT);}
    avatar(dc,x-12,y-17+bob,24,h->seed,c);
    if(h->boots_level){fill(dc,x-10,y+3+bob,7,5,RGB(125,182,221));fill(dc,x+4,y+3+bob,7,5,RGB(125,182,221));}
    if(kind==1) {fill(dc,x-12,y-18+bob,24,4,c);fill(dc,x-7,y-24+bob,14,6,c);fill(dc,x-2,y-29+bob,5,6,c);fill(dc,x+13,y-10,3,27,RGB(135,107,77));fill(dc,x+10,y-15,9,8,LILAC);}
    else if(kind==0) {fill(dc,x-10,y-20+bob,20,6,RGB(177,194,190));fill(dc,x-15,y-23+bob,5,9,RGB(192,205,191));fill(dc,x+11,y-23+bob,5,9,RGB(192,205,191));}
    else {fill(dc,x-14,y-20+bob,28,4,c);fill(dc,x-8,y-24+bob,17,5,c);fill(dc,x+7,y-29+bob,3,9,GOLD);}
    if(h->attack>0&&model.connected) {
        int reach=(int)((.28f-h->attack)*85);
        if(kind==1) {ellipse_fill(dc,x+15+reach,y-8,10,10,LILAC);fill(dc,x+12+reach,y-5,3,3,INK);}
        else if(kind==0) {fill(dc,x+12,y-3,27,4,INK);fill(dc,x+19,y-12,4,20,GOLD);fill(dc,x+34,y-8,5,4,INK);}
        else {fill(dc,x+14+reach,y-3,14,2,GOLD);fill(dc,x+26+reach,y-5,3,6,INK);}
    } else if(h->state==WORKING&&kind!=1){fill(dc,x+14,y-6,3,20,RGB(191,207,202));fill(dc,x+11,y+7,9,3,GOLD);}
    if(h->state==WAITING) {int lift=(int)(sinf(world_time*4)*3);panel(dc,x-8,y-51+lift,18,21,RGB(67,53,31),GOLD,5);label(dc,x-8,y-51+lift,18,21,L"!",5,GOLD,DT_CENTER|DT_SINGLELINE);}
    if(h->state==FAILED)label(dc,x-8,y-43,22,22,L"+",2,RGB(255,136,144),DT_SINGLELINE);
    if(h->shop_state==2) {wchar_t shop[100];_snwprintf(shop,99,L"%ls?  %dG",upgrade_names[h->shop_choice],upgrade_cost(h,h->shop_choice));panel(dc,x-67,y-65,137,23,RGB(59,49,31),GOLD,5);label(dc,x-65,y-62,133,19,shop,0,GOLD,DT_CENTER|DT_SINGLELINE);}
    if(h->purchase_fx>0){wchar_t bought[100];int lift=(int)((2.3f-h->purchase_fx)*11);_snwprintf(bought,99,L"-%dG  %ls +1",h->last_price,upgrade_names[h->shop_choice]);label(dc,x-85,y-58-lift,175,22,bought,0,GOLD,DT_CENTER|DT_SINGLELINE);}
    i=h->model_index;if(i>=0&&i<model.count) {
        int left=x-80;if(left<MAP_X+4)left=MAP_X+4;if(left>MAP_X+MAP_W-168)left=MAP_X+MAP_W-168;
        wcsncpy(text,model.agents[i].name,22);text[22]=0;
        if(!wcscmp(text,L"Untitled quest"))wcscpy(text,L"Companion");
        panel(dc,left,y+23,164,20,RGB(20,36,32),RGB(37,60,48),4);
        dot(dc,left+7,y+30,state_color(h->state));label(dc,left+22,y+26,135,17,text,0,chosen?INK:RGB(170,193,173),DT_SINGLELINE|DT_END_ELLIPSIS);
    }
    if(hero_hit_count<MAX_HEROES) {hero_hits[hero_hit_count]=(RECT){x-20,y-32,x+23,y+43};hero_slots[hero_hit_count++]=(int)(h-heroes);}
}
static void world_map(HDC dc) {
    int i,order[MAX_HEROES],n=0,j;float flicker=sinf(world_time*9);
    terrain_create(dc);StretchBlt(dc,MAP_X,MAP_Y,MAP_W,MAP_H,terrain_dc,0,0,MAP_W,MAP_H,SRCCOPY);
    /* Small fire and water glints keep camp alive without an asset engine. */
    fill(dc,MAP_X+118,MAP_Y+410,14,14,RGB(205,111,62));fill(dc,MAP_X+121,MAP_Y+402+(int)(flicker*3),8,18,RGB(246,174,86));fill(dc,MAP_X+124,MAP_Y+408,3,10,RGB(252,222,125));
    fill(dc,MAP_X+640+(int)(sinf(world_time)*10),MAP_Y+450,22,2,RGB(94,147,131));
    hero_hit_count=0;
    for(i=0;i<MAX_HEROES;i++)if(heroes[i].used) {
        if(heroes[i].state==WORKING)monster_draw(dc,&heroes[i]);
        j=n;while(j>0&&heroes[order[j-1]].y>heroes[i].y){order[j]=order[j-1];j--;}order[j]=i;n++;
    }
    for(i=0;i<n;i++)hero_draw(dc,&heroes[order[i]]);
    if(!model.connected) {panel(dc,MAP_X+215,MAP_Y+200,380,85,RGB(26,32,38),GOLD,10);label(dc,MAP_X+231,MAP_Y+216,350,25,L"The guild signal is lost.",2,GOLD,DT_CENTER|DT_SINGLELINE);label(dc,MAP_X+231,MAP_Y+250,350,25,L"Last-known heroes are paused.",0,MUTED,DT_CENTER|DT_SINGLELINE);}
}
static void draw_world(HDC dc) {
    int i,working=0,waiting=0,idle=0;wchar_t text[400];Agent *a=NULL;Hero *h=NULL;
    fill(dc,0,0,1180,800,BG);fill(dc,0,0,1180,3,MINT);
    panel(dc,28,26,40,40,RGB(35,57,48),RGB(53,85,68),10);avatar(dc,36,34,24,0,MINT);
    label(dc,82,23,350,32,L"AGENT GUILD",2,INK,DT_SINGLELINE);label(dc,83,55,460,20,L"A LIVING WORLD FOR YOUR AI PARTY",0,MUTED,DT_SINGLELINE);
    panel(dc,900,28,250,40,PANEL,LINE,10);dot(dc,916,44,model.connected?MINT:GOLD);label(dc,934,38,200,24,model.connected?L"LIVE CODEX HOST":L"HOST UNAVAILABLE",0,model.connected?MINT:GOLD,DT_SINGLELINE);
    fill(dc,28,91,1124,1,LINE);label(dc,28,108,600,35,L"Small heroes. Real quests.",2,INK,DT_SINGLELINE);
    label(dc,605,115,233,24,L"CLICK A HERO TO FOLLOW",0,MUTED,DT_RIGHT|DT_SINGLELINE);
    world_map(dc);
    for(i=0;i<model.count;i++) {
        Agent *item=&model.agents[i];if(item->state==WORKING)working++;if(item->state==WAITING)waiting++;if(item->state==IDLE)idle++;
        if(!wcscmp(item->id,selected_id)){a=item;h=hero_for(item->id);}
    }
    panel(dc,860,155,292,520,PANEL,LINE,12);label(dc,881,174,245,23,L"QUEST LOG",0,MUTED,DT_SINGLELINE);
    if(a) {
        avatar(dc,882,214,48,id_hash(a->id),h?hero_color(h):state_color(a->state));
        label(dc,947,214,180,25,a->child?L"Party companion":L"Quest leader",5,INK,DT_SINGLELINE);
        label(dc,947,246,180,24,states[a->state],0,state_color(a->state),DT_SINGLELINE);
        label(dc,881,293,248,65,a->name,2,INK,DT_WORDBREAK|DT_END_ELLIPSIS);
        panel(dc,881,363,248,49,RGB(47,42,29),RGB(87,72,44),9);
        fill(dc,893,379,13,17,GOLD);fill(dc,890,383,19,9,GOLD);fill(dc,898,382,3,10,RGB(255,236,151));
        _snwprintf(text,399,L"%d GOLD",h?h->gold:0);label(dc,920,373,165,31,text,h&&h->gold>999?1:2,GOLD,DT_SINGLELINE);
        if(h){_snwprintf(text,399,L"KEEP %dG",gold_reserve(h));label(dc,1035,389,86,18,text,0,MUTED,DT_RIGHT|DT_SINGLELINE);}
        {int goal=h?(h->shop_state?h->shop_choice:choose_upgrade(h,0)):-1;if(goal>=0)_snwprintf(text,399,L"%ls %ls (%dG)",h->shop_state?L"BUYING":L"SAVING FOR",upgrade_names[goal],upgrade_cost(h,goal));else wcscpy(text,L"ALL UPGRADES MAXED");label(dc,881,424,248,21,text,0,MUTED,DT_SINGLELINE);}
        for(i=0;i<3;i++) {
            int sx=881+i*85;panel(dc,sx,451,77,62,RGB(28,34,43),LINE,7);
            label(dc,sx,459,77,20,upgrade_names[i],0,MUTED,DT_CENTER|DT_SINGLELINE);
            _snwprintf(text,399,L"%d",h?upgrade_level(h,i):0);label(dc,sx,478,77,29,text,2,i==0?MINT:i==1?LILAC:GOLD,DT_CENTER|DT_SINGLELINE);
        }
        label(dc,881,527,248,21,L"IN THE WORLD",0,MUTED,DT_SINGLELINE);
        if(!model.connected)wcscpy(text,L"Signal lost - waiting to reconnect");
        else if(h&&a->state==WORKING) {
            if(h->shop_state)_snwprintf(text,399,L"%ls %ls upgrade (%dG)",h->shop_state==2?L"Choosing":L"Visiting merchant for",upgrade_names[h->shop_choice],upgrade_cost(h,h->shop_choice));
            else if(h->respawn>0)_snwprintf(text,399,L"Monster defeated! +%d gold",h->last_reward);
            else if(h->fighting)_snwprintf(text,399,L"Fighting a %ls",monsters[h->monster_kind]);
            else _snwprintf(text,399,L"Travelling to %ls",regions[h->region]);
        } else if(a->state==WAITING)wcscpy(text,L"Calling for help at camp");
        else wcscpy(text,L"Taking a campfire break");
        text[399]=0;label(dc,881,555,248,52,text,1,a->state==WAITING?GOLD:MINT,DT_WORDBREAK);
        if(h) {
            if(h->buff_time>0)_snwprintf(text,399,L"%ls  %ds  /  KOs %d",buff_names[h->buff_kind],(int)ceilf(h->buff_time),h->kills);
            else _snwprintf(text,399,L"MONSTER KOs  %d",h->kills);
            label(dc,881,614,248,21,text,0,GOLD,DT_SINGLELINE);
        }
        label(dc,881,645,248,20,basename_w(a->cwd),0,MUTED,DT_SINGLELINE|DT_END_ELLIPSIS);
    } else {avatar(dc,882,218,48,3,LILAC);label(dc,881,302,248,65,L"Your party awaits.",2,INK,DT_WORDBREAK);label(dc,881,395,248,130,model.connected?L"Start a Codex task. Its hero will leave camp and find a monster to battle.":model.message,1,MUTED,DT_WORDBREAK);}
    for(i=0;i<3;i++)filter_hits[i]=(RECT){0,0,0,0};
    row_count=0;if(scroll_row>=model.count)scroll_row=model.count?model.count-1:0;if(scroll_row<0)scroll_row=0;
    for(i=scroll_row;i<model.count&&row_count<5;i++) {
        int x=28+row_count*164,chosen=!wcscmp(selected_id,model.agents[i].id);Agent *item=&model.agents[i];
        panel(dc,x,695,154,61,chosen?RGB(30,49,41):PANEL,chosen?RGB(70,115,88):LINE,10);
        avatar(dc,x+10,706,24,id_hash(item->id),state_color(item->state));
        label(dc,x+43,705,102,22,item->name,0,INK,DT_SINGLELINE|DT_END_ELLIPSIS);
        label(dc,x+10,735,135,18,states[item->state],0,state_color(item->state),DT_SINGLELINE);
        row_hits[row_count]=(RECT){x,695,x+154,756};row_indices[row_count++]=i;
    }
    panel(dc,860,695,292,61,PANEL,LINE,10);
    if(model.connected)_snwprintf(text,399,L"%d questing   %d waiting   %d camp",working,waiting,idle);else wcscpy(text,L"Disconnected - reconnecting...");
    label(dc,877,707,263,23,text,0,model.connected?MINT:GOLD,DT_SINGLELINE);
    _snwprintf(text,399,L"%d loaded  /  scroll the party",model.count);label(dc,877,734,263,20,text,0,MUTED,DT_SINGLELINE);
    fill(dc,28,771,1124,1,LINE);label(dc,29,780,720,18,L"Live states  /  In-game gold & upgrades  /  Space to pause",0,MUTED,DT_SINGLELINE);
    if(world_count()<model.count){_snwprintf(text,399,L"Map: %d/%d",world_count(),model.count);label(dc,750,780,170,18,text,0,GOLD,DT_SINGLELINE);}
    refresh_hit=(RECT){1014,773,1152,800};label(dc,1014,780,138,18,L"Refresh  [R]",0,MINT,DT_SINGLELINE|DT_RIGHT);
}
static void draw(HDC dc) {
    int i;const wchar_t *views[]={L"World",L"Roster"};
    if(world_view)draw_world(dc);else draw_roster(dc);
    for(i=0;i<2;i++) {
        int x=580+i*90,chosen=world_view==(i==0);view_hits[i]=(RECT){x,30,x+82,65};
        panel(dc,x,30,82,35,chosen?RGB(36,61,49):PANEL,chosen?RGB(61,105,82):LINE,9);
        label(dc,x,39,82,22,views[i],0,chosen?MINT:MUTED,DT_CENTER|DT_SINGLELINE);
    }
    pause_hit=(RECT){773,30,862,65};
    if(world_view){panel(dc,773,30,89,35,PANEL,LINE,9);label(dc,773,39,89,22,paused?L"Resume":L"Pause",0,paused?GOLD:MUTED,DT_SINGLELINE|DT_CENTER);}
}
#endif
