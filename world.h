/* Small status-driven simulation. All battles are decorative, never task progress. */
#ifndef GUILD_WORLD_H
#define GUILD_WORLD_H
#include <math.h>
#define MAX_HEROES 96
typedef struct {
    wchar_t id[100];
    unsigned int seed;
    int used, seen, state, region, monster_kind, model_index, hp, max_hp, fighting, hits, kills;
    int gold,power_level,boots_level,luck_level,spent,shop_state,shop_choice,last_damage,last_reward,buff_kind,last_price,last_shop_review,last_purchase_kills;
    float x,y,tx,ty,mx,my,step,attack,impact,cooldown,rest,respawn;
    float buff_time,shop_timer,purchase_fx,loot_fx,loot_x,loot_y,shopping_cooldown;
} Hero;
static unsigned int world_rng=1;
static unsigned int random_next(void);
#include "economy.h"
static Hero heroes[MAX_HEROES];
static float world_time;
static const float region_x[]={170,380,650,590,375,170};
static const float region_y[]={125,95,135,355,310,265};
static const wchar_t *regions[]={L"Mossfall Grove",L"Emberroot Hollow",L"Moonfern Glade",L"Silverleaf Shore",L"Oldstone Ruins",L"Bramble Pass"};
static const wchar_t *monsters[]={L"Moss Muncher",L"Twig Goblin",L"Acorn Bandit"};
static unsigned int random_next(void) {
    unsigned int x=world_rng?world_rng:1;x^=x<<13;x^=x>>17;x^=x<<5;world_rng=x;return x;
}
static float jitter(int radius) {return (float)((int)(random_next()%(radius*2+1))-radius);}
static void encounter(Hero *h) {
    h->region=(int)(random_next()%6);h->monster_kind=(int)(random_next()%3);
    h->mx=region_x[h->region]+jitter(42);h->my=region_y[h->region]+jitter(32);
    h->tx=h->mx-35;h->ty=h->my+4;
    h->max_hp=h->hp=4+(int)(random_next()%4)+h->power_level;h->fighting=0;h->shop_state=0;
    h->cooldown=.45f;h->attack=h->impact=h->respawn=0;
}
static void camp_target(Hero *h) {
    if(h->state==WAITING) {h->tx=210+jitter(25);h->ty=416+jitter(21);}
    else {h->tx=65+(h->seed%6)*22+jitter(8);h->ty=367+((h->seed/6)%4)*25+jitter(7);}
    h->fighting=0;h->hp=0;h->shop_state=0;h->attack=h->impact=h->respawn=0;h->rest=2+(random_next()%40)*.1f;
}
static Hero *hero_for(const wchar_t *id) {
    int i;for(i=0;i<MAX_HEROES;i++)if(heroes[i].used&&!wcscmp(heroes[i].id,id))return &heroes[i];return NULL;
}
static void world_sync(void) {
    int i,k,slot;for(i=0;i<MAX_HEROES;i++)heroes[i].seen=0;
    /* First retain existing slots so sorting changes don't respawn characters. */
    for(i=0;i<model.count;i++) {Hero *h=hero_for(model.agents[i].id);if(h)h->seen=1;}
    for(i=0;i<MAX_HEROES;i++)if(heroes[i].used&&!heroes[i].seen)memset(&heroes[i],0,sizeof(Hero));
    for(i=0;i<model.count;i++) {
        Agent *a=&model.agents[i];Hero *h=hero_for(a->id);int fresh=0;
        if(a->state==OFFLINE)continue;
        if(!h) {
            for(slot=0;slot<MAX_HEROES&&heroes[slot].used;slot++){}if(slot==MAX_HEROES)break;
            h=&heroes[slot];memset(h,0,sizeof(*h));wcscpy(h->id,a->id);h->used=1;h->seed=id_hash(a->id);h->state=-1;
            h->x=65+(slot%6)*23;h->y=390+(slot/6%4)*22;fresh=1;
            economy_apply(h);
        }
        h->seen=1;h->model_index=i;
        if(fresh||h->state!=a->state) {h->state=a->state;if(h->state==WORKING)encounter(h);else camp_target(h);}
    }
}
static int world_count(void) {int n=0,i;for(i=0;i<MAX_HEROES;i++)if(heroes[i].used)n++;return n;}
static void world_step(float dt) {
    int i;
    if(!model.connected)return;
    if(dt<0)dt=0;if(dt>.1f)dt=.1f;world_time+=dt;
    for(i=0;i<MAX_HEROES;i++)if(heroes[i].used) {
        Hero *h=&heroes[i];float dx=h->tx-h->x,dy=h->ty-h->y,distance=sqrtf(dx*dx+dy*dy);
        h->attack=fmaxf(0,h->attack-dt);h->impact=fmaxf(0,h->impact-dt);
        h->buff_time=fmaxf(0,h->buff_time-dt);h->purchase_fx=fmaxf(0,h->purchase_fx-dt);h->loot_fx=fmaxf(0,h->loot_fx-dt);h->shopping_cooldown=fmaxf(0,h->shopping_cooldown-dt);
        if(h->shop_state==2) {
            h->shop_timer-=dt;
            if(h->shop_timer<=0){economy_buy(h,h->shop_choice);if(h->state==WORKING)encounter(h);else camp_target(h);}
            continue;
        }
        if(h->respawn>0) {
            h->respawn-=dt;
            if(h->respawn<=0) {
                int choice=shop_decision(h);
                if(choice>=0){h->shop_state=1;h->shop_choice=choice;h->tx=271;h->ty=399;h->hp=0;h->fighting=0;}
                else encounter(h);
            }
            continue;
        }
        if(distance>1.5f) {
            float speed=h->state==WORKING?75.0f:43.0f,travel=fminf(distance,speed*dt);
            speed*=1+h->boots_level*.12f;if(h->buff_kind==2&&h->buff_time>0)speed*=1.6f;travel=fminf(distance,speed*dt);
            h->x+=dx/distance*travel;h->y+=dy/distance*travel;h->step+=dt*10;h->fighting=0;
        } else if(h->shop_state==1) {
            h->shop_state=2;h->shop_timer=2.0f;h->fighting=0;
        } else if(h->state==WORKING) {
            h->fighting=1;h->cooldown-=dt;
            if(h->cooldown<=0) {
                h->cooldown=.7f+(h->seed%4)*.09f;if(h->buff_kind==2&&h->buff_time>0)h->cooldown*=.55f;
                h->attack=.28f;h->impact=.32f;h->hits++;h->last_damage=1+h->power_level+(h->buff_kind==1&&h->buff_time>0?2:0);h->hp-=h->last_damage;
                if(h->hp<=0) {
                    int roll=random_next()%100,bonus=random_next()%100,reward=gold_drop(h,roll,bonus);
                    h->kills++;h->gold+=reward;if(h->gold>1000000)h->gold=1000000;h->last_reward=reward;h->loot_fx=1.8f;h->loot_x=h->mx;h->loot_y=h->my;
                    if(random_next()%10==0){h->buff_kind=1+random_next()%3;h->buff_time=12.0f;}
                    h->respawn=1.0f;h->fighting=0;economy_record(h);
                }
            }
        } else {
            h->rest-=dt;
            if(h->rest<=0)camp_target(h);
        }
    }
}
static int world_test(void) {
    int i,hits;float before;Hero *active,*waiting,*idle;
    memset(&model,0,sizeof(model));memset(heroes,0,sizeof(heroes));economy_reset();world_rng=12345;model.connected=1;model.count=3;
    wcscpy(model.agents[0].id,L"test-working");model.agents[0].state=WORKING;
    wcscpy(model.agents[1].id,L"test-waiting");model.agents[1].state=WAITING;
    wcscpy(model.agents[2].id,L"test-idle");model.agents[2].state=IDLE;
    world_sync();active=hero_for(L"test-working");waiting=hero_for(L"test-waiting");idle=hero_for(L"test-idle");
    before=active->x;for(i=0;i<3000;i++)world_step(1.0f/30.0f);
    if(active->x==before||active->hits<8||active->kills<1||waiting->hits||idle->hits)return 51;
    if(active->gold<0||active->gold+active->spent<=0)return 56;
    if(active->x<20||active->x>780||active->y<30||active->y>490)return 52;
    hits=active->hits;model.agents[0].state=WAITING;world_sync();for(i=0;i<300;i++)world_step(1.0f/30.0f);
    if(active->hits!=hits||active->attack||active->hp)return 53;
    before=active->x;model.connected=0;world_step(.1f);if(active->x!=before)return 54;
    model.count=0;world_sync();if(world_count())return 55;
    memset(&model,0,sizeof(model));memset(heroes,0,sizeof(heroes));economy_reset();world_time=0;world_rng=GetTickCount()|1;return economy_test();
}
#endif
