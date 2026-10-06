// Included by native_surface.cpp. Read-only catalogue browser: no sack, item factory,
// view requests or journal writes. Own navigation never changes the caravan page.
namespace {
bool g_viewer=false, g_viewerSearch=false, g_viewerOwned=false;
bool g_viewerHelp=false;
unsigned g_viewerScopeOwned=0,g_viewerScopeTotal=0;
bool g_viewerSets=false;
bool g_viewerFavorites=false,g_viewerFavoritesWritable=true;
std::set<std::string> g_viewerFavoriteRecords;
wchar_t g_viewerFavoritesPath[MAX_PATH]={};
std::vector<int> g_viewerCategories;
struct ViewerSet { int group,owned,total; };
std::vector<ViewerSet> g_viewerSetEntries;
std::vector<unsigned char> g_viewerCategoryMatches;
std::string g_viewerMatchQuery;
DWORD g_viewerMatchesAt=0;
unsigned g_viewerWorld=0;
DWORD g_viewerClosedAt=0;
int g_viewerGroup=0,g_viewerCategoryTop=0,g_viewerRow=0;
int g_viewerDetail=-1,g_viewerDetailScroll=0,g_viewerDetailMax=0;
UtWheelAcc g_viewerWheel;
unsigned g_viewerCollected=0,g_viewerTotal=0;
std::wstring g_viewerQuery;
struct ViewerEntry { const gdut::ItemView* info; unsigned count; int group,entry; };
std::vector<ViewerEntry> g_viewerEntries;
struct ViewerTexture { const TqTexture* texture=nullptr; bool tried=false; };
std::map<std::string,ViewerTexture> g_viewerTextures;
museum::ui::Rect g_viewerRect;
float g_viewerScale=1;
constexpr int kViewerPerPage=24;
constexpr int kViewerCategoryRows=15;
constexpr float kViewerBodyTop=88.0f,kViewerBodyHeight=500.0f;
constexpr float kViewerCategoryStep=(kViewerBodyHeight+4.0f)/kViewerCategoryRows;
int viewerColumns() { return g_viewerSets?2:6; }
int viewerWindowRows() { return g_viewerSets?6:4; }
size_t viewerEntryCount() { return g_viewerSets?g_viewerSetEntries.size():g_viewerEntries.size(); }
int viewerTotalRows() { return (int(viewerEntryCount())+viewerColumns()-1)/viewerColumns(); }

void viewerFavoritesLoad() {
    g_viewerFavoriteRecords.clear();g_viewerFavoritesWritable=true;
    wchar_t leaf[256]={};
    const char* set=journalSetLeaf();
    wchar_t setWide[192]={};
    if(!set || !MultiByteToWideChar(CP_UTF8,0,set,-1,setWide,192)) {
        g_viewerFavoritesWritable=false;return;
    }
    swprintf_s(leaf,L"%s-favorites.txt",setWide);
    utModPathW(nullptr,leaf,g_viewerFavoritesPath,MAX_PATH);
    const DWORD attrs=GetFileAttributesW(g_viewerFavoritesPath);
    if(attrs==INVALID_FILE_ATTRIBUTES && GetLastError()==ERROR_FILE_NOT_FOUND)return;
    WIN32_FILE_ATTRIBUTE_DATA size={};
    if(!GetFileAttributesExW(g_viewerFavoritesPath,GetFileExInfoStandard,&size) ||
       size.nFileSizeHigh || size.nFileSizeLow>1024*1024) {
        g_viewerFavoritesWritable=false;logW("viewer: favorites file unreadable or oversized; preserved");return;
    }
    std::ifstream file(g_viewerFavoritesPath,std::ios::binary);
    if(!file){g_viewerFavoritesWritable=false;logW("viewer: favorites cannot be read; writes disabled");return;}
    std::string line;size_t bytes=0;
    while(std::getline(file,line)) {
        bytes+=line.size()+1;
        if(!line.empty() && line.back()=='\r')line.pop_back();
        if(line.empty())continue;
        if(bytes>1024*1024 || line.size()>255 || line.find_first_of(" \t")!=std::string::npos ||
           line.find('\0')!=std::string::npos || line.compare(0,8,"records/")!=0) {
            g_viewerFavoritesWritable=false;break;
        }
        g_viewerFavoriteRecords.insert(line);
    }
    if(file.bad())g_viewerFavoritesWritable=false;
    if(!g_viewerFavoritesWritable)logW("viewer: invalid favorites file; preserved, writes disabled");
}
bool viewerFavoriteToggle(const std::string& record) {
    if(!g_viewerFavoritesWritable)return false;
    auto next=g_viewerFavoriteRecords;
    if(!next.erase(record))next.insert(record);
    std::string data;for(const auto& key:next){data+=key;data+='\n';}
    wchar_t temp[MAX_PATH]={};
    if(_snwprintf_s(temp,_TRUNCATE,L"%s.tmp",g_viewerFavoritesPath)<0)return false;
    HANDLE file=CreateFileW(temp,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;const DWORD size=DWORD(data.size());
    const bool ok=WriteFile(file,data.data(),size,&written,nullptr) && written==size && FlushFileBuffers(file);
    const bool closed=CloseHandle(file)!=FALSE;
    if(!ok || !closed || !MoveFileExW(temp,g_viewerFavoritesPath,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp);logW("viewer: favorites write failed; previous list retained");return false;
    }
    g_viewerFavoriteRecords.swap(next);return true;
}

std::wstring viewerWide(const char* text) {
    wchar_t out[1024]={};
    if(text) MultiByteToWideChar(CP_UTF8,0,text,-1,out,1024);
    return out;
}
std::string viewerNeedle() {
    return utSearchNeedleWide(reinterpret_cast<const unsigned short*>(g_viewerQuery.c_str()),g_viewerQuery.size());
}
bool viewerRecordMatch(int group,int entry,const std::string& query) {
    if(query.empty())return false;
    const char* record=liveGroupRecord(group,entry);
    const auto* info=liveItemInfo(record);
    if(info && utSearchHit(utSearchNeedleUtf8(std::string(info->name).c_str()),query))return true;
    if(searchViewerMatch(group,entry,query))return true;
    const char* source=tooltipBestSourceName(record);
    if(source && utSearchHit(utSearchNeedleUtf8(source),query))return true;
    TooltipSourceText parts;
    return tooltipSourceText(record,0,parts) &&
        (utSearchHit(utSearchNeedleUtf8(parts.details),query) || utSearchHit(utSearchNeedleUtf8(parts.chance),query));
}
void viewerRebuild() {
    g_viewerDetail=-1;g_viewerDetailScroll=0;searchViewerTooltipClear();
    g_viewerEntries.clear();g_viewerSetEntries.clear();g_viewerRow=0;g_viewerWheel.sum=0;
    g_viewerScopeOwned=0;g_viewerScopeTotal=0;
    g_viewerMatchesAt=0;
    if(g_viewerSets) {
        for(int group=0;group<liveGroupCount();++group) {
            if(museum::categorySection(liveGroupLabel(group))!=museum::Section::Sets)continue;
            int owned=0;const int total=liveGroupEntries(group);
            for(int i=0;i<total;++i)if(journalRows(liveGroupRecord(group,i)))++owned;
            ++g_viewerScopeTotal;if(total>0 && owned==total)++g_viewerScopeOwned;
            if(g_viewerOwned && !owned)continue;
            g_viewerSetEntries.push_back({group,owned,total});
        }
        return;
    }
    const int groups=g_viewerFavorites?int(g_viewerCategories.size()):1;
    std::set<std::string> seen;
    for(int g=0;g<groups;++g) {
      const int group=g_viewerFavorites?g_viewerCategories[size_t(g)]:g_viewerGroup;
      for(int i=0;i<liveGroupEntries(group);++i) {
        const char* record=liveGroupRecord(group,i);
        if(g_viewerFavorites && !g_viewerFavoriteRecords.count(record))continue;
        if(g_viewerFavorites && !seen.insert(record).second)continue;
        const auto* info=liveItemInfo(record);
        if(!info)continue;
        const unsigned count=journalRows(record);
        ++g_viewerScopeTotal;if(count)++g_viewerScopeOwned;
        if(g_viewerOwned && !count)continue;
        g_viewerEntries.push_back({info,count,group,i});
      }
    }
}
void viewerClose() {
    if(!g_viewer)return;
    g_viewer=false;g_viewerSearch=false;g_viewerHelp=false;g_viewerClosedAt=GetTickCount();
    searchViewerTooltipClear();
    logI("viewer: closed");
}
bool viewerPlayer() {
    bool result=false;
    __try { auto* ge=gameEngine();result=ge && g_tq.GameGetMainPlayer && g_tq.GameGetMainPlayer(ge)!=nullptr; }
    __except(EXCEPTION_EXECUTE_HANDLER) { result=false; }
    return result;
}
bool viewerInputBlocked() {
    return g_viewer || (g_viewerClosedAt && GetTickCount()-g_viewerClosedAt<250);
}
void viewerToggle() {
    if(g_viewer){viewerClose();return;}
    if(!hookKeyGateLive() || !hookMouseGateLive() || !viewerPlayer() || !journalSetKnown() ||
       !liveActive() || hookCaravanOpen() || viewOn())return;
    searchFieldBlur("opening the read-only viewer");
    g_viewerWorld=viewWorldGeneration();g_viewer=true;
    viewerFavoritesLoad();
    g_viewerCategories.clear();
    for(int group=0;group<liveGroupCount();++group)
        if(museum::categorySection(liveGroupLabel(group))!=museum::Section::Sets)g_viewerCategories.push_back(group);
    g_viewerCollected=0;g_viewerTotal=0;
    for(int group:g_viewerCategories)for(int k=0;k<liveGroupEntries(group);++k) {
        ++g_viewerTotal;
        if(journalRows(liveGroupRecord(group,k)))++g_viewerCollected;
    }
    g_viewerGroup=(std::min)(g_viewerGroup,liveGroupCount()-1);
    viewerRebuild();logI("viewer: opened (key %d), read-only",g_cfg.viewerHotkey);
}
bool viewerHit(float x,float y,float rx,float ry,float w,float h) {
    return x>=rx && x<rx+w && y>=ry && y<ry+h;
}
void viewerRowStep(int dir,bool wholeWindow=false) {
    const int last=(std::max)(0,viewerTotalRows()-viewerWindowRows());
    g_viewerRow=(std::max)(0,(std::min)(last,g_viewerRow+dir*(wholeWindow?viewerWindowRows():1)));
}
void viewerCategoryStep(int dir) {
    g_viewerCategoryTop=(std::max)(0,(std::min)((std::max)(0,int(g_viewerCategories.size())-kViewerCategoryRows),g_viewerCategoryTop+dir));
}
bool viewerInput(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_KEYDOWN && g_cfg.viewerHotkey && wp==WPARAM(g_cfg.viewerHotkey) &&
       !searchFieldFocused() && !(g_viewer && g_viewerSearch)) {
        if(!(lp&(1L<<30)))viewerToggle();
        return g_viewer || viewerInputBlocked();
    }
    if(!g_viewer)return false;
    if(msg==WM_KILLFOCUS || msg==WM_CANCELMODE){viewerClose();return false;}
    if(msg==WM_KEYDOWN) {
        if(wp==VK_ESCAPE){if(g_viewerHelp)g_viewerHelp=false;else viewerClose();}
        else if(!g_viewerSearch && wp==VK_NEXT)viewerRowStep(1,true);
        else if(!g_viewerSearch && wp==VK_PRIOR)viewerRowStep(-1,true);
        return true;
    }
    POINT p;GetCursorPos(&p);ScreenToClient(hwnd,&p);
    RECT client={};GetClientRect(hwnd,&client);
    PlateGeometry geo={};if(!plateGeometry(&geo) || client.right<=0 || client.bottom<=0)return true;
    const float x=(p.x*float(geo.canvasW)/client.right-g_viewerRect.x)/g_viewerScale;
    const float y=(p.y*float(geo.canvasH)/client.bottom-g_viewerRect.y)/g_viewerScale;
    if(msg==WM_MOUSEWHEEL) {
        if(g_viewerHelp)return true;
        const int dir=-utWheelAccumulate(g_viewerWheel,GET_WHEEL_DELTA_WPARAM(wp));
        if(x<190)viewerCategoryStep(dir*3);
        else if(viewerHit(x,y,750,88,282,500))g_viewerDetailScroll=(std::max)(0,(std::min)(g_viewerDetailMax,g_viewerDetailScroll+dir*3));
        else viewerRowStep(dir);
        return true;
    }
    if(msg==WM_LBUTTONDOWN) {
        if(viewerHit(x,y,968,12,28,28)){g_viewerHelp=!g_viewerHelp;g_viewerSearch=false;return true;}
        if(g_viewerHelp){g_viewerHelp=false;return true;}
        g_viewerSearch=viewerHit(x,y,202,44,338,28);
        if(viewerHit(x,y,512,44,28,28)){g_viewerQuery.clear();g_viewerSearch=true;return true;}
        if(viewerHit(x,y,1004,12,28,28))viewerClose();
        else if(viewerHit(x,y,548,44,91,28)){g_viewerOwned=!g_viewerOwned;viewerRebuild();}
        else if(viewerHit(x,y,647,44,91,28)){g_viewerFavorites=true;g_viewerSets=false;viewerRebuild();}
        else if(viewerHit(x,y,12,44,176,30)){g_viewerSets=true;g_viewerFavorites=false;viewerRebuild();}
        else if(viewerHit(x,y,12,kViewerBodyTop,176,kViewerBodyHeight)) {
            const int index=g_viewerCategoryTop+int((y-kViewerBodyTop)/kViewerCategoryStep);
            if(index<int(g_viewerCategories.size())){g_viewerGroup=g_viewerCategories[size_t(index)];g_viewerSets=false;g_viewerFavorites=false;viewerRebuild();}
        }
        else if(viewerHit(x,y,212,98,516,480)) {
            if(g_viewerSets) {
                const int index=g_viewerRow*2+int((y-98)/80)*2+int((x-212)/258);
                if(index<int(g_viewerSetEntries.size())) {
                    g_viewerGroup=g_viewerSetEntries[size_t(index)].group;g_viewerSets=false;
                    g_viewerQuery.clear();viewerRebuild();
                }
                return true;
            }
            const int slot=int((y-98)/120)*6+int((x-212)/86),index=g_viewerRow*6+slot;
            const float sx=212+float(slot%6)*86,sy=98+float(slot/6)*120;
            if(index<int(g_viewerEntries.size()) && viewerHit(x,y,sx+63,sy+3,20,20)) {
                const std::string record(g_viewerEntries[size_t(index)].info->record);
                if(viewerFavoriteToggle(record) && g_viewerFavorites) {
                    const int row=g_viewerRow;viewerRebuild();
                    g_viewerRow=(std::min)(row,(std::max)(0,viewerTotalRows()-viewerWindowRows()));
                }
            }
        }
    }
    return (msg>=WM_LBUTTONDOWN && msg<=WM_MBUTTONDBLCLK) || msg==WM_MOUSEWHEEL;
}
const TqTexture* viewerLoadTexture(const TqStdString* name) {
    __try { auto* e=engine();auto* gfx=e?g_tq.EngineGetGraphicsEngine(e):nullptr;
        return gfx?g_tq.GfxLoadTexture(gfx,name):nullptr;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
void viewerDrawTexture(TqCanvas* canvas,const TqTexture* texture,float x,float y,float s,bool collected,bool small) {
    __try {
        const int tw=g_tq.TextureGetWidth(texture),th=g_tq.TextureGetHeight(texture);
        if(tw>0 && th>0) {
            float fit=(std::min)(60.0f/tw,78.0f/th);
            if(small)fit=(std::min)(fit,1.35f);
            // Center within the icon area, below the star and above the unknown marker.
            const TqRect target={g_viewerRect.x+(x+43-tw*fit*0.5f)*s,g_viewerRect.y+(y+60-th*fit*0.5f)*s,tw*fit*s,th*fit*s};
            const TqRect source={0,0,float(tw),float(th)};
            const TqColor tint=collected?TqColor{1,1,1,1}:TqColor{0,0,0,1};
            g_tq.CanvasRenderRectTex(canvas,&target,&source,texture,&tint,nullptr,nullptr);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
}
const TqTexture* viewerTexture(const gdut::ItemView& info,bool& loaded) {
    if(info.bitmap.empty() || !g_tq.GfxLoadTexture)return nullptr;
    auto& cached=g_viewerTextures[std::string(info.bitmap)];
    if(!cached.tried && !loaded) {
        loaded=true;cached.tried=true;
        TqStdString name;char heap[512];
        if(tqStdStringOver(&name,heap,sizeof(heap),info.bitmap.data(),info.bitmap.size())) {
            cached.texture=viewerLoadTexture(&name);
        }
    }
    return cached.texture;
}
const TqTexture* g_viewerStarTextures[2]={};
bool g_viewerStarSource=false,g_viewerStarOff=false,g_viewerStarTried=false;
bool viewerStarAddSource(const TqStdString* dir) {
    __try { auto* e=engine();void* fs=e?g_tq.EngineGetFileSystem(e):nullptr;
        return fs && g_tq.FsAddSource(fs,1,dir,nullptr,0,0,0);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void viewerStarPrepare(bool noWorldYet) {
    if(g_viewerStarSource || g_viewerStarOff || !noWorldYet)return;
    if(!engine() || !g_tq.EngineGetFileSystem || !g_tq.FsAddSource)return;
    wchar_t folder[MAX_PATH]={};utModPathW(nullptr,L"viewer-ui",folder,MAX_PATH);
    CreateDirectoryW(folder,nullptr);
    float vx[10],vy[10],minX=100,maxX=-100,minY=100,maxY=-100;
    for(int i=0;i<10;++i) {
        const float angle=float(i)*0.62831853f-1.57079633f,radius=(i%2)?3.8f:8.0f;
        vx[i]=std::cos(angle)*radius;vy[i]=std::sin(angle)*radius;
        minX=(std::min)(minX,vx[i]);maxX=(std::max)(maxX,vx[i]);
        minY=(std::min)(minY,vy[i]);maxY=(std::max)(maxY,vy[i]);
    }
    for(int i=0;i<10;++i){vx[i]=(vx[i]-minX)*16/(maxX-minX);vy[i]=(vy[i]-minY)*16/(maxY-minY);}
    auto inside=[&](float px,float py,float shrink) {
        bool hit=false;
        for(int i=0,j=9;i<10;j=i++) {
            const float ax=8+(vx[i]-8)*shrink,ay=8+(vy[i]-8)*shrink;
            const float bx=8+(vx[j]-8)*shrink,by=8+(vy[j]-8)*shrink;
            if(((ay>py)!=(by>py)) && px<(bx-ax)*(py-ay)/(by-ay)+ax)hit=!hit;
        }
        return hit;
    };
    bool ok=true;
    for(int variant=0;variant<2 && ok;++variant) {
        std::vector<unsigned char> bytes(kViewerStarHeader,kViewerStarHeader+sizeof(kViewerStarHeader));
        for(int row=0;row<96;++row)for(int col=0;col<64;++col) {
            unsigned coverage=0;
            for(int sy=0;sy<4;++sy)for(int sx=0;sx<4;++sx) {
                const float px=(float(col)+(float(sx)+0.5f)/4)*16/64;
                const float py=(float(row)+(float(sy)+0.5f)/4)*16/96;
                if(inside(px,py,1) && (variant || !inside(px,py,0.68f)))++coverage;
            }
            bytes.push_back(255);bytes.push_back(255);bytes.push_back(255);bytes.push_back(static_cast<unsigned char>((coverage*255+8)/16));
        }
        wchar_t path[MAX_PATH]={};
        _snwprintf_s(path,_TRUNCATE,L"%s\\museum-star-%d.tex",folder,variant);
        HANDLE file=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE){ok=false;break;}
        DWORD written=0;ok=WriteFile(file,bytes.data(),DWORD(bytes.size()),&written,nullptr) && written==bytes.size();
        if(!CloseHandle(file))ok=false;
    }
    char directory[MAX_PATH+2]={};
    if(!WideCharToMultiByte(CP_ACP,0,folder,-1,directory,MAX_PATH,nullptr,nullptr))ok=false;
    const size_t length=strlen(directory);directory[length]='/';directory[length+1]=0;
    for(char* p=directory;*p;++p)if(*p=='\\')*p='/';
    TqStdString name;char heap[MAX_PATH+2];
    if(ok)ok=tqStdStringOver(&name,heap,sizeof(heap),directory,strlen(directory)) && viewerStarAddSource(&name);
    g_viewerStarSource=ok;g_viewerStarOff=!ok;
    logI("viewer: star textures %s",ok?"generated; native directory source registered":"unavailable");
}
void viewerStar(TqCanvas* canvas,float x,float y,float scale,bool favorite,const TqColor&) {
    if(!g_viewerStarSource || !g_tq.CanvasRenderRectTex)return;
    if(!g_viewerStarTried) {
        g_viewerStarTried=true;
        for(int variant=0;variant<2;++variant) {
            char text[32];sprintf_s(text,"museum-star-%d.tex",variant);
            TqStdString name;char heap[32];
            if(tqStdStringOver(&name,heap,sizeof(heap),text,strlen(text)))g_viewerStarTextures[variant]=viewerLoadTexture(&name);
        }
    }
    const auto* texture=g_viewerStarTextures[favorite?1:0];if(!texture)return;
    __try {
        const TqRect target={float(std::round(x)),float(std::round(y)),14*scale,14*scale};
        const TqRect source={0,0,float(g_tq.TextureGetWidth(texture)),float(g_tq.TextureGetHeight(texture))};
        const TqColor color=favorite?TqColor{0.95f,0.77f,0.35f,1}:TqColor{0.67f,0.64f,0.53f,1};
        g_tq.CanvasRenderRectTex(canvas,&target,&source,texture,&color,nullptr,nullptr);
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
}
void viewerDraw() {
    if(!g_viewer)return;
    if(g_viewerWorld!=viewWorldGeneration() || !viewerPlayer() || !journalSetKnown() || hookCaravanOpen()) {
        viewerClose();return;
    }
    PlateGeometry geo={};if(!plateGeometry(&geo))return;
    auto* canvas=canvasNow();if(!canvas || !g_tq.CanvasRenderRect)return;
    ensureFont();
    using namespace museum::ui;
    museum::game::NativeRenderer renderer(canvas,g_font);
    g_viewerScale=(std::min)(geo.scale,(std::min)((geo.canvasW-24)/1044.0f,(geo.canvasH-24)/630.0f));
    const float s=g_viewerScale;
    g_viewerRect={(geo.canvasW-1044*s)*0.5f,(geo.canvasH-630*s)*0.5f,1044*s,630*s};
    auto rect=[&](float x,float y,float w,float h){return Rect{g_viewerRect.x+x*s,g_viewerRect.y+y*s,w*s,h*s};};
    auto label=[&](float x,float y,float w,float h,const std::wstring& text,Color color=Color{0.85f,0.78f,0.56f,1},TextAlign align=TextAlign::Left) {
        std::wstring fitted=text;
        const int size=(std::max)(1,int((std::min)(14.0f,h-2)*s));
        if(renderer.measure(fitted.c_str(),size)>(w-2)*s) {
            while(!fitted.empty() && renderer.measure((fitted+L"...").c_str(),size)>(w-2)*s)fitted.pop_back();
            fitted+=L"...";
        }
        renderer.text(rect(x,y,w,h),fitted.c_str(),size,color,align);
    };
    auto button=[&](float x,float y,float w,const std::wstring& text,bool on=false,float h=28,bool marked=false) {
        renderer.fill(rect(x,y,w,h),marked?Color{0.227f,0.384f,0.533f,1}:on?Color{0.48f,0.42f,0.25f,1}:Color{0.30f,0.24f,0.14f,1});
        label(x+6,y,w-12,h,text,marked?Color{0.91f,0.918f,0.929f,1}:Color{0.85f,0.78f,0.56f,1},TextAlign::Center);
        if(marked && on)renderer.outline(rect(x,y,w,h),{0.85f,0.78f,0.56f,1},s);
    };
    renderer.fill(g_viewerRect,{0.08f,0.07f,0.05f,1});renderer.outline(g_viewerRect,{0.68f,0.62f,0.44f,1},s);
    const Color panelEdge={0.27f,0.23f,0.15f,1};
    renderer.fill(rect(202,kViewerBodyTop,536,kViewerBodyHeight),{0.10f,0.09f,0.065f,1});
    renderer.outline(rect(202,kViewerBodyTop,536,kViewerBodyHeight),panelEdge,s);
    renderer.fill(rect(750,kViewerBodyTop,282,kViewerBodyHeight),{0.12f,0.105f,0.075f,1});
    renderer.outline(rect(750,kViewerBodyTop,282,kViewerBodyHeight),panelEdge,s);
    label(12,4,948,28,viewerWide(museum::i18n::text("museum.viewer.title")));
    button(968,12,28,L"?",g_viewerHelp);
    button(1004,12,28,L"X");
    renderer.fill(rect(202,44,338,28),{0.180f,0.165f,0.122f,1});
    renderer.outline(rect(202,44,338,28),g_viewerSearch?Color{0.85f,0.78f,0.56f,1}:Color{0.48f,0.42f,0.25f,1},s);
    std::wstring searchText=g_viewerQuery;
    if(g_viewerSearch && (GetTickCount()/500u)%2u==0)searchText+=L'_';
    while(!searchText.empty() && renderer.measure(searchText.c_str(),int(14*s))>294*s)searchText.erase(0,1);
    if(searchText.empty() && !g_viewerSearch)searchText=viewerWide(museum::i18n::text("museum.search"));
    label(210,44,294,28,searchText);
    if(!g_viewerQuery.empty())label(512,44,28,28,L"x",{0.85f,0.78f,0.56f,1},TextAlign::Center);
    button(548,44,91,viewerWide(museum::i18n::text("museum.owned")),g_viewerOwned);
    button(647,44,91,viewerWide(museum::i18n::text("museum.viewer.favorites")),g_viewerFavorites);
    button(12,44,176,viewerWide(museum::i18n::text("museum.section.sets")),!g_viewerFavorites && (g_viewerSets || museum::categorySection(liveGroupLabel(g_viewerGroup))==museum::Section::Sets));
    const auto needle=viewerNeedle();
    if(needle!=g_viewerMatchQuery || !g_viewerMatchesAt || GetTickCount()-g_viewerMatchesAt>=500) {
        g_viewerMatchQuery=needle;g_viewerMatchesAt=GetTickCount();
        g_viewerCategoryMatches.assign(g_viewerCategories.size(),0);
        if(!needle.empty())for(size_t i=0;i<g_viewerCategories.size();++i) {
            const int group=g_viewerCategories[i];
            for(int k=0;k<liveGroupEntries(group);++k)if(viewerRecordMatch(group,k,needle)) {
                g_viewerCategoryMatches[i]=1;break;
            }
        }
    }
    for(int row=0;row<kViewerCategoryRows;++row) {
        const int index=g_viewerCategoryTop+row;if(index>=int(g_viewerCategories.size()))break;
        const int group=g_viewerCategories[size_t(index)];
        const float y=kViewerBodyTop+row*kViewerCategoryStep,h=kViewerCategoryStep-4;
        const bool marked=size_t(index)<g_viewerCategoryMatches.size() && g_viewerCategoryMatches[size_t(index)];
        button(12,y,176,viewerWide(museum::categoryCaption(liveGroupLabel(group))),!g_viewerFavorites && !g_viewerSets && group==g_viewerGroup,h,marked);
    }
    POINT p={};RECT client={};HWND hwnd=hookGameWindow();
    GetCursorPos(&p);ScreenToClient(hwnd,&p);GetClientRect(hwnd,&client);
    const float mx=client.right>0?(p.x*float(geo.canvasW)/client.right-g_viewerRect.x)/s:-1;
    const float my=client.bottom>0?(p.y*float(geo.canvasH)/client.bottom-g_viewerRect.y)/s:-1;
    int hoveredItem=-1;bool loaded=false;
    if(g_viewerSets)for(int slot=0;slot<12;++slot) {
        const int index=g_viewerRow*2+slot;if(index>=int(g_viewerSetEntries.size()))break;
        const auto& set=g_viewerSetEntries[size_t(index)];
        const float x=212+float(slot%2)*258,y=98+float(slot/2)*80;
        const bool hovered=viewerHit(mx,my,x,y,258,80);
        renderer.fill(rect(x+3,y+3,252,74),hovered?Color{0.38f,0.32f,0.19f,1}:Color{0.20f,0.17f,0.11f,1});
        label(x+10,y+8,238,30,viewerWide(museum::categoryCaption(liveGroupLabel(set.group))),{0.85f,0.78f,0.56f,1},TextAlign::Center);
        wchar_t progress[64];swprintf_s(progress,L"%d / %d",set.owned,set.total);
        label(x+10,y+42,238,26,progress,{0.65f,0.63f,0.57f,1},TextAlign::Center);
        bool matched=!needle.empty() && utSearchHit(utSearchNeedleUtf8(museum::categoryCaption(liveGroupLabel(set.group))),needle);
        for(int k=0;!matched && k<set.total;++k)matched=viewerRecordMatch(set.group,k,needle);
        if(matched)renderer.outline(rect(x+3,y+3,252,74),{0.38f,0.79f,1,1},2*s);
    }
    for(int slot=0;!g_viewerSets && slot<kViewerPerPage;++slot) {
        const int index=g_viewerRow*6+slot;if(index>=int(g_viewerEntries.size()))break;
        const auto& entry=g_viewerEntries[size_t(index)];const auto& info=*entry.info;
        const float x=212+float(slot%6)*86,y=98+float(slot/6)*120;
        const Color color=info.classification==gdut::Classification::Legendary?Color{0.62f,0.32f,0.88f,1}:Color{0.18f,0.64f,0.92f,1};
        renderer.fill(rect(x+3,y+3,80,114),entry.count?Color{color.r*0.18f,color.g*0.18f,color.b*0.18f,1}:Color{0.18f,0.16f,0.12f,1});
        const bool matched=viewerRecordMatch(entry.group,entry.entry,needle);
        if(matched)renderer.fill(rect(x+3,y+3,80,114),{0.15f,0.30f,0.40f,1});
        if(viewerHit(mx,my,x,y,86,120))hoveredItem=index;
        if(index==hoveredItem)renderer.outline(rect(x+3,y+3,80,114),color,s);
        const auto* texture=viewerTexture(info,loaded);
        if(texture && g_tq.CanvasRenderRectTex && g_tq.TextureGetWidth && g_tq.TextureGetHeight) {
            viewerDrawTexture(canvas,texture,x,y,s,entry.count!=0,info.footW==1 && info.footH==1);
        }
        if(!entry.count)label(x+8,y+98,70,18,L"???",{0.5f,0.48f,0.42f,1});
        if(matched)renderer.outline(rect(x+3,y+3,80,114),{0.38f,0.79f,1,1},2*s);
        const bool favorite=g_viewerFavoriteRecords.count(std::string(info.record))!=0;
        const TqColor background=matched?TqColor{0.15f,0.30f,0.40f,1}:entry.count?TqColor{color.r*0.18f,color.g*0.18f,color.b*0.18f,1}:TqColor{0.18f,0.16f,0.12f,1};
        viewerStar(canvas,g_viewerRect.x+(x+66)*s,g_viewerRect.y+(y+6)*s,s,favorite,background);
    }



    const float footerCenter=(kViewerBodyTop+kViewerBodyHeight+630.0f)*0.5f;
    wchar_t count[64];swprintf_s(count,L"%d\x2013%d / %d  (%zu)",viewerTotalRows()?g_viewerRow+1:0,
        (std::min)(viewerTotalRows(),g_viewerRow+viewerWindowRows()),viewerTotalRows(),viewerEntryCount());
    label(202,footerCenter-7,536,14,count,{0.65f,0.63f,0.57f,1},TextAlign::Center);
    swprintf_s(count,L"%u / %u",g_viewerScopeOwned,g_viewerScopeTotal);
    label(12,footerCenter-15,176,14,viewerWide(museum::i18n::text(g_viewerFavorites?"museum.viewer.favorites":g_viewerSets?"museum.section.sets":"museum.viewer.category"))+L": "+count,{0.85f,0.78f,0.56f,1},TextAlign::Center);
    swprintf_s(count,L"%u / %u",g_viewerCollected,g_viewerTotal);
    label(12,footerCenter+1,176,14,viewerWide(museum::i18n::text("museum.all"))+L": "+count,{0.62f,0.60f,0.54f,1},TextAlign::Center);
    const int difficulty=tooltipSourceDifficulty();
    if(difficulty>=0 && difficulty<3) {
        static const char* const keys[]={"museum.difficulty.normal","museum.difficulty.epic","museum.difficulty.legendary"};
        char caption[128];snprintf(caption,sizeof(caption),museum::i18n::text("museum.viewer.difficulty"),museum::i18n::text(keys[difficulty]));
        label(750,footerCenter-7,282,14,viewerWide(caption),{0.65f,0.63f,0.57f,1},TextAlign::Center);
    }
    wchar_t keyName[64]={};
    if(g_cfg.viewerHotkey) {
        if(g_cfg.viewerHotkey>='A' && g_cfg.viewerHotkey<='Z')keyName[0]=wchar_t(g_cfg.viewerHotkey);
        else GetKeyNameTextW(LONG(MapVirtualKeyW(UINT(g_cfg.viewerHotkey),MAPVK_VK_TO_VSC)<<16),keyName,64);
    }
    if(hoveredItem>=0) {
        if(g_viewerDetail!=hoveredItem){g_viewerDetailScroll=0;g_viewerDetailMax=0;}
        g_viewerDetail=hoveredItem;
    } else if(!viewerHit(mx,my,738,88,294,500))g_viewerDetail=-1;
    const int chosen=g_viewerDetail;
    if(chosen>=0 && chosen<int(g_viewerEntries.size())) {
        const auto& entry=g_viewerEntries[size_t(chosen)];const auto& info=*entry.info;
        const std::string name(info.name),record(info.record);
        if(entry.count) {
            const auto* lines=searchViewerTooltip(record.c_str());
            static std::vector<ViewerTooltipLine> wrapped;
            static std::string wrappedRecord;
            static float wrappedScale=0;
            static bool wrappedReady=false;
            std::wstring title=viewerWide(name.c_str());
            Color titleColor={0.85f,0.78f,0.56f,1};
            if(lines)for(const auto& line:*lines) {
                if(line.cls>=0x02 && line.cls<=0x0D) {
                    title=line.text;
                    if(line.cls==0x04)titleColor={0.25f,1,0.25f,1};
                    else if(line.cls==0x05)titleColor={0,0.64f,1,1};
                    else if(line.cls==0x06)titleColor={0.85f,0.02f,1,1};
                    break;
                }
            }
            label(760,96,262,28,title,titleColor);
            if(!lines){wrapped.clear();wrappedReady=false;}
            if(lines && (!wrappedReady || wrappedRecord!=record || wrappedScale!=s)) {
              wrapped.clear();wrappedRecord=record;wrappedScale=s;wrappedReady=true;
              for(const auto& line:*lines) {
                // The main name is a fixed header, shared with the unknown-item layout.
                if(line.cls>=0x01 && line.cls<=0x0D)continue;
                std::wstring rest=line.text;
                while(!rest.empty()) {
                    size_t n=rest.size();
                    while(n>1 && renderer.measure(rest.substr(0,n).c_str(),int(14*s))>262*s)--n;
                    if(n<rest.size()) {
                        const size_t space=rest.rfind(L' ',n);
                        if(space!=std::wstring::npos && space>0)n=space;
                    }
                    wrapped.push_back({rest.substr(0,n),line.cls});rest.erase(0,n);
                    while(!rest.empty() && rest.front()==L' ')rest.erase(0,1);
                }
              }
            }
            constexpr int visible=19;
            g_viewerDetailMax=(std::max)(0,int(wrapped.size())-visible);
            g_viewerDetailScroll=(std::min)(g_viewerDetailScroll,g_viewerDetailMax);
            if(!lines) {
                label(760,136,262,22,viewerWide(museum::i18n::text(searchViewerTooltipPending()?"museum.viewer.stats.pending":"museum.viewer.stats.unavailable")));
            }
            for(int row=0;row<visible && row+g_viewerDetailScroll<int(wrapped.size());++row) {
                const auto& line=wrapped[size_t(row+g_viewerDetailScroll)];
                Color tint={0.85f,0.78f,0.56f,1};
                if(line.cls==0x0F)tint={1,1,1,1};
                else if(line.cls==0x10 || line.cls==0x1A)tint={0,0.64f,1,1};
                else if(line.cls==0x12 || line.cls==0x16)tint={0.25f,1,0.25f,1};
                else if(line.cls==0x04)tint={0.25f,1,0.25f,1};
                else if(line.cls==0x05)tint={0,0.64f,1,1};
                else if(line.cls==0x06)tint={0.85f,0.02f,1,1};
                else if(line.cls==kUtSearchClsRequirements)tint={0.65f,0.63f,0.57f,1};
                label(760,136+row*22.0f,262,22,line.text,tint);
            }
            if(g_viewerDetailMax) {
                swprintf_s(count,L"%d / %zu",g_viewerDetailScroll+1,wrapped.size());
                label(760,562,262,16,count,{0.65f,0.63f,0.57f,1},TextAlign::Right);
            }
        } else {
            label(760,96,262,28,L"???");
            TooltipSourceText source;int rows=0;
            for(int i=0;i<10 && tooltipSourceText(record.c_str(),size_t(i),source);++i) {
                const float y=136+float(i)*44;
                label(760,y,262,22,viewerWide(source.name));
                const auto chance=viewerWide(source.chance);
                const float chanceWidth=renderer.measure(chance.c_str(),int(14*s))/s+4;
                label(760,y+22,254-chanceWidth,20,viewerWide(source.details),{0.62f,0.60f,0.54f,1});
                label(1022-chanceWidth,y+22,chanceWidth,20,chance,{0.95f,0.77f,0.35f,1},TextAlign::Right);++rows;
            }
            if(!rows)label(760,136,262,22,viewerWide(tooltipSourceStatus()));
        }
    }
    if(g_viewerHelp) {
        renderer.fill(rect(650,48,382,108),{0.12f,0.10f,0.06f,1});
        renderer.outline(rect(650,48,382,108),{0.68f,0.62f,0.44f,1},s);
        label(662,58,358,26,std::wstring(keyName)+(keyName[0]?L" / ":L"")+viewerWide(museum::i18n::text("museum.viewer.help.close")));
        label(662,88,358,26,viewerWide(museum::i18n::text("museum.viewer.help.wheel")));
        label(662,118,358,26,viewerWide(museum::i18n::text("museum.viewer.help.hover")));
    }
}
struct ViewerKey { int button,state;unsigned short text[16]; };
bool viewerReadKey(const void* event,ViewerKey* key) {
    __try {
        const auto* bytes=static_cast<const unsigned char*>(event);
        key->button=*reinterpret_cast<const int*>(bytes+0x0C);
        key->state=*reinterpret_cast<const int*>(bytes+0x10);
        memset(key->text,0,sizeof(key->text));
        if(g_viewer && g_viewerSearch && key->state!=kUtKeyStateRelease && g_tq.ButtonEventGetText) {
            const auto* text=g_tq.ButtonEventGetText(event);
            for(int i=0;text && i<15 && text[i];++i)key->text[i]=text[i];
        }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}
bool panelViewerActive() { return viewerInputBlocked(); }
bool panelViewerKeyGate(const void* event) {
    if(!viewerInputBlocked())return false;
    ViewerKey key={};if(!viewerReadKey(event,&key))return g_viewer;
    // Preserve release bookkeeping; presses are claimed while the viewer is open.
    if(key.state==kUtKeyStateRelease)return false;
    if(!viewerInputBlocked())return false;
    if(g_viewer && g_viewerSearch && key.state==kUtKeyStatePress) {
        bool changed=false;
        if(key.button==kUtKeyBack && !g_viewerQuery.empty()){g_viewerQuery.pop_back();changed=true;}
        else for(int i=0;i<15 && key.text[i] && g_viewerQuery.size()<80;++i)
            if(key.text[i]>=0x20){g_viewerQuery+=wchar_t(key.text[i]);changed=true;}
        if(changed)g_viewerMatchesAt=0;
    }
    return true;
}
