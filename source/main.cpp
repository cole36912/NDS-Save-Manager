/*---------------------------------------------------------------------------------

	NDS Save Manager
    Version 1.0
    Cole36912

---------------------------------------------------------------------------------*/
#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <set>
#include <map>
#include <vector>
#include <cctype>
#include <cstring>
#include <errno.h>

using namespace std;
volatile int frame = 0;
unsigned int stops = 1;

const string GAME_PATH =        "/Games/";
const string SAVE_PATH =        GAME_PATH + "other_saves/";

const char* C_GAME_PATH =       GAME_PATH.c_str();
const char* C_SAVE_PATH =       SAVE_PATH.c_str();

const char* COLOR_DEFAULT =     "\x1b[37;1m";
const char* COLOR_TITLE =       "\x1b[31;1m";
const char* COLOR_SELECTION =   "\x1b[34;1m";
const char* COLOR_SELECTED =    "\x1b[32;1m";
const char* COLOR_SELECTIONED = "\x1b[36;1m";

const mode_t REGULAR =          S_IRWXU | S_IRWXG | S_IRWXO;

void stop (void) {
    //iprintf("Stop %u: Press A to continue\n", stops);
    stops++;
    while (1) {
        swiWaitForVBlank();
        scanKeys();
        int pressed = keysDown();
        if ( pressed & KEY_A)return;
    }
}

string tolower(string s){
    for(unsigned int i = 0; i < s.size(); i++)s[i] = tolower(s[i]);
    return s;
}

void OnKeyPressed(int key) {
    if(key > 0)
        iprintf("%c", key);
    
}

string keybdin(){
    Keyboard *kbd = keyboardDemoInit();
    kbd->OnKeyPressed = OnKeyPressed;
    char myName[256];
    scanf("%s", myName);
    consoleClear();
    return string(myName);
}

PrintConsole* initop(){
    PrintConsole* topScreen;
    consoleInit(topScreen, 3,BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    return topScreen;
}

PrintConsole* inikbd(){
    return consoleDemoInit();
}

void createblank(string s, bool full){
    char f = 0xFF;
    int fildes =open(s.c_str(),O_WRONLY | O_CREAT | O_TRUNC,REGULAR);
    if(full){
        char* f8 = new char[1<<19];
        memset(f8,f,1<<19);
        write(fildes, f8, (1<<19));
        delete f8;
    }
    else write(fildes, &f, 1);
    close(fildes);
    return;
}

void Vblank() {
	frame++;
    return;
}

string getitle(string path){
    int fildes =open(path.c_str(),O_RDONLY);
    lseek(fildes,0x68,SEEK_SET);
    char* c = new char[100];
    read(fildes,c,4);
    unsigned int next = c[0] + (c[1]*(1<<8)) + (c[2]*(1<<16)) + (c[3]*(1<<24)) + 832;
    lseek(fildes,next,SEEK_SET);
    read(fildes,c,100);
    string s;
    for(unsigned int i = 0; (i < 100 /*&& c[i] != 0x0A*/); i+=2)s.push_back(((c[i] < 32) || (c[i] > 126)) ? 32 : c[i]);
    delete c;
    close(fildes);
    return s;
}

unsigned int min(unsigned int x, unsigned int y){
    if(x>y)return y;
    else return x;
}

void deletefile(string s){
    remove(s.c_str());
    return;
}

int main() {

    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);
    
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);


	irqSet(IRQ_VBLANK, Vblank);

    initop();
    if (!fatInitDefault()) {
        iprintf ("\nfatinitDefault failed!\n");
        stop();
    }
    struct stat sb;
    string dirs[2] = {GAME_PATH.substr(0,GAME_PATH.size()-1),SAVE_PATH.substr(0,SAVE_PATH.size()-1)};
    if (!(stat(dirs[0].c_str(), &sb) == 0 && S_ISDIR(sb.st_mode)))iprintf("Creating Directory \"%s\"...%s\n",dirs[0].c_str(),mkdir(dirs[0].c_str(), REGULAR) == 0 ? "Success" : string("Error:"+to_string(errno)).c_str());
    if (!(stat(dirs[1].c_str(), &sb) == 0 && S_ISDIR(sb.st_mode)))iprintf("Creating Directory \"%s\"...%s\n",dirs[1].c_str(),mkdir(dirs[1].c_str(), REGULAR) == 0 ? "Success" : string("Error:"+to_string(errno)).c_str());

    DIR* dir;
    
    dir = opendir (C_GAME_PATH);
    
    if (dir == NULL) {
        iprintf ("Unable to open the directory.\n");
    } else {
        struct dirent* dirr;
        set<string> games;
        set<string> cursave;
        map<string,vector<string> > saves;
        map<string,pair<string,string> > ori;
        map<string,string> titles;
        while ((dirr = readdir(dir))) {
            string s = string(dirr->d_name);
            string subs = tolower(s.substr(0,s.size()-4));
            if(subs.size() >= 7)subs = subs.substr(0,7);
            else while(subs.size() < 7)subs += "0";
            if(s.size() >= 4 && s[0] != '.' && tolower(s.substr(s.size()-4, 4)) == string(".nds")){
                games.insert(subs);
                if(ori.find(subs) != ori.end())ori[subs].first = s;
                else ori.emplace(subs, pair<string,string>(s, ""));
            }
            else if(s.size() >= 4 && s[0] != '.' && tolower(s.substr(s.size()-4, 4)) == string(".sav")){
                cursave.insert(subs);
                if(ori.find(subs) != ori.end())ori[subs].second = s;
                else ori.emplace(subs, pair<string,string>("", s));
            }
        }
        dir = opendir(C_SAVE_PATH);
        if (dir == NULL) {
            iprintf ("Unable to open the directory.\n");
            stop();
        }
        
        while ((dirr = readdir(dir))) {
            string s = string(dirr->d_name);
            string subs = tolower(s.substr(0,s.size()-4));
            if(subs.size() >= 7)subs = subs.substr(0,7);
            else while(subs.size() < 7)subs += "0";
            string name;
            if(s.size() >= 12)name = s.substr(8,s.size()- 4 - 8);
            else name = "";
            if(s.size() >= 4 && s[0] != '.' && tolower(s.substr(s.size()-4, 4)) == string(".cur")){
                if(saves.find(subs) == saves.end())saves.emplace(subs, vector<string>({name, name}));
                else saves[subs][1] = saves[subs][0] = name;
            }
            else if(s.size() >= 4 && s[0] != '.' && tolower(s.substr(s.size()-4, 4)) == string(".sav")){
                if(saves.find(subs) == saves.end())saves.emplace(subs, vector<string>({"","",name}));
                else saves[subs].push_back(name);
            }
        }
        for(auto it = games.begin(); it != games.end(); it++)if(saves.find(*it) == saves.end()){
            if(cursave.find(*it) == cursave.end())saves.emplace(*it, vector<string>());
            else{
                saves.emplace(*it, vector<string>({"save 1", "save 1"}));
                createblank(SAVE_PATH+*it+"-save 1"+".cur",false);
            }
        }
        for(auto it = games.begin(); it != games.end(); it++)titles.emplace(*it,getitle(GAME_PATH+ori[*it].first));
        auto gamit = games.begin();
        auto gamlast = games.end();
        gamlast--;
        unsigned int savesel = 1;
        bool inside = false;
        bool lastin = true;
        unsigned int basav = 1;
        auto bagam = games.begin();
        bool title = 1;
        while(1) {
            swiWaitForVBlank();
            iprintf("\x1b[2J");
            if(inside){
                iprintf("%s\x1b[0;0H%s%s\x1b[0;30H%s\x1b[1;0H",COLOR_TITLE, gamit == games.begin() ? "  " : "<-",(!title ? ori[*gamit].first : titles[*gamit]).substr(0,28).c_str(), gamit == gamlast ? "  " : "->");
                for(unsigned int i = basav; i < min(saves[*gamit].size(),22+basav); i++)iprintf("%s%s\n", saves[*gamit][i] == saves[*gamit][0] ? (i == savesel ? COLOR_SELECTIONED : COLOR_SELECTION) : (i == savesel ? COLOR_SELECTED : COLOR_DEFAULT), saves[*gamit][i].c_str());
                if(!lastin){
                    inikbd();
                    iprintf("\x1b[2J");
                    iprintf("\x1b[0;0H%s",COLOR_DEFAULT);
                    iprintf("(a) Select\n");
                    iprintf("(b) Back\n");
                    iprintf("(x) New Save\n");
                    iprintf("(y) Rename\n");
                    iprintf("(select) Toggle Display\n");
                    iprintf("(l)+(r)+(y) Delete\n");
                    initop();
                }
            }
            else{
                for(auto i = bagam; distance(bagam, i) != 23; i++)iprintf("\x1b[%u;0H%s%s", distance(bagam, i), (i == gamit ? COLOR_SELECTED : COLOR_DEFAULT), (!title ? ori[*i].first : titles[*i]).substr(0,32).c_str());
                if(lastin){
                    inikbd();
                    iprintf("\x1b[2J");
                    iprintf("\x1b[0;0H%s",COLOR_DEFAULT);
                    iprintf("(a) Select\n");
                    iprintf("(select) Toggle Display\n");
                    initop();
                }
            }
            lastin = inside;
            scanKeys();
            int pressed = keysDown();
            int held = keysHeld();
            if (((inside ? pressed : held) & KEY_LEFT) && gamit != games.begin()){
                /*for(unsigned int i = 0; i < (inside ? 1 : 5) && gamit != games.begin(); i++)*/gamit--;
                savesel = 1;
            }
            else if (((inside ? pressed : held) & KEY_RIGHT) && gamit != gamlast){
                /*for(unsigned int i = 0; i < (inside ? 1 : 5) && gamit != gamlast; i++)*/gamit++;
                savesel = 1;
            }
            else if (((pressed) & KEY_DOWN) && !inside && gamit != gamlast)gamit++;
            else if (((pressed) & KEY_UP) && !inside && gamit != games.begin())gamit--;
            else if ((pressed & KEY_DOWN) && savesel != saves[*gamit].size()-1){
                savesel++;
                if(savesel - basav > 21)basav++;
            }
            else if ((pressed & KEY_UP) && savesel != 1){
                savesel--;
                if(basav > savesel)basav--;
            }
            else if ((pressed & KEY_Y) && !(held & KEY_L)){
                inikbd();
                iprintf("\x1b[2J");
                string nname = keybdin();
                if(saves[*gamit][savesel] == saves[*gamit][0]){
                    saves[*gamit][0] = nname;
                    rename(string(SAVE_PATH+*gamit+"-"+saves[*gamit][savesel]+".cur").c_str(),string(SAVE_PATH+*gamit+"-"+nname+".cur").c_str());
                }
                else rename(string(SAVE_PATH+*gamit+"-"+saves[*gamit][savesel]+".sav").c_str(),string(SAVE_PATH+*gamit+"-"+nname+".sav").c_str());
                saves[*gamit][savesel] = nname;
                initop();
                lastin = false;
            }
            else if (pressed & KEY_X){
                if(saves[*gamit].empty()){
                    saves[*gamit] = vector<string>({"save 1", "save 1"});
                    createblank(SAVE_PATH+*gamit+"-save 1"+".cur",false);
                    createblank(GAME_PATH+tolower((ori[*gamit].first).substr(0,(ori[*gamit].first).size()-4))+".sav", true);
                }
                else{
                    int num = 1;
                    for(bool found = false; !found; num++){
                        found = true;
                        for(unsigned int i = 1; i < saves[*gamit].size(); i++)if(saves[*gamit][i] == "save "+to_string(num))found = false;
                    }
                    saves[*gamit].push_back("save "+to_string(num - 1));
                    createblank(SAVE_PATH+*gamit+"-save "+to_string(num - 1)+".sav",true);
                }
            }
            else if ((pressed & KEY_A) && !inside)inside = true;
            else if ((pressed & KEY_SELECT))title = !title;
            else if ((pressed & KEY_B)){
                inside = false;
                savesel = 1;
            }
            else if ((pressed & KEY_A) && saves[*gamit][savesel] != saves[*gamit][0]){
                rename(string(GAME_PATH+ori[*gamit].second).c_str(),string(SAVE_PATH+*gamit+"-"+saves[*gamit][0]+".sav").c_str());
                rename(string(SAVE_PATH+*gamit+"-"+saves[*gamit][savesel]+".sav").c_str(),string(GAME_PATH+ori[*gamit].second).c_str());
                rename(string(SAVE_PATH+*gamit+"-"+saves[*gamit][0]+".cur").c_str(), string(SAVE_PATH+*gamit+"-"+saves[*gamit][savesel]+".cur").c_str());
                saves[*gamit][0] = saves[*gamit][savesel];
            }
            else if (((KEY_Y|KEY_R|KEY_L) == held) && inside && !saves[*gamit].empty()){
                inikbd();
                consoleClear();
                if(savesel == 1){
                    iprintf("You cannot delete the selected save.\n(a) okay");
                    while(!(pressed & KEY_A)){
                        swiWaitForVBlank();
                        scanKeys();
                        pressed = keysDown();
                    }
                }
                else{
                    iprintf("Delete %s?\n(a) yes\n(b) no",saves[*gamit][savesel].c_str());
                    while(1){
                        if(pressed & KEY_A){
                            deletefile(SAVE_PATH+*gamit+"-"+saves[*gamit][savesel]+".sav");
                            saves[*gamit].erase(saves[*gamit].begin()+savesel);
                            break;
                        }
                        else if (pressed & KEY_B)break;
                        swiWaitForVBlank();
                        scanKeys();
                        pressed = keysDown();
                    }
                }
                initop();
                lastin = false;
            }
            while(distance(games.begin(),bagam) > distance(games.begin(),gamit))bagam--;
            while(distance(bagam, gamit) > 22)bagam++;
            
        }
    }
	return 0;
}
