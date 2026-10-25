// Update flow follows rtk19/NX-torrent-player app/update.cpp:
// release prompt -> changelog / download progress -> verified staging -> relaunch.
#include "utils/app_updater.hpp"
#include "utils/config_helper.hpp"
#include "utils/release_version.hpp"
#include "utils/update_files.hpp"
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/dialog.hpp>
#include <borealis/views/label.hpp>
#include <borealis/views/progress_spinner.hpp>
#include <borealis/views/scrolling_frame.hpp>
#include <curl/curl.h>
#include <atomic>
#include <cstdio>
#include <memory>
#include <thread>
#ifdef __SWITCH__
#include <switch.h>
#endif

namespace tsvitch {
namespace {
constexpr uint64_t maximumSize=128*1024*1024;
constexpr const char* releaseApi="https://api.github.com/repos/rtk19/TsVitch/releases/latest";
std::string executablePath;
std::atomic<bool> busy{false}, stopping{false}, pending{false};
std::thread worker;
struct Release {std::string version,title,notes,url,digest;uint64_t size=0;};
std::string tr(const std::string& key){return brls::getStr("iptv/"+key);}
void note(const std::string& message){auto* d=new brls::Dialog(message);d->addButton(brls::getStr("hints/ok"),[]{});d->open();}
void dispatch(std::function<void()> callback){if(!stopping)brls::sync([callback]{if(!stopping)callback();});}
void start(std::function<void()> job){if(worker.joinable())worker.join();worker=std::thread(std::move(job));}
using Curl=std::unique_ptr<CURL,decltype(&curl_easy_cleanup)>;
Curl request(const std::string& url) {
    Curl c(curl_easy_init(),curl_easy_cleanup);
    if(!c)throw std::runtime_error("Cannot start network request");
    curl_easy_setopt(c.get(),CURLOPT_URL,url.c_str());
    curl_easy_setopt(c.get(),CURLOPT_USERAGENT,"TsVitch-rtk19-updater");
    curl_easy_setopt(c.get(),CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(c.get(),CURLOPT_MAXREDIRS,5L);
    curl_easy_setopt(c.get(),CURLOPT_PROTOCOLS,CURLPROTO_HTTPS);
    curl_easy_setopt(c.get(),CURLOPT_REDIR_PROTOCOLS,CURLPROTO_HTTPS);
    curl_easy_setopt(c.get(),CURLOPT_CONNECTTIMEOUT,20L);
    curl_easy_setopt(c.get(),CURLOPT_LOW_SPEED_LIMIT,1024L);
    curl_easy_setopt(c.get(),CURLOPT_LOW_SPEED_TIME,30L);
    curl_easy_setopt(c.get(),CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(c.get(),CURLOPT_NOPROGRESS,0L);
    return c;
}
void perform(CURL* c) {
    const auto code=curl_easy_perform(c);long status=0;
    curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&status);
    if(code!=CURLE_OK)throw std::runtime_error(curl_easy_strerror(code));
    if(status!=200)throw std::runtime_error("HTTP "+std::to_string(status));
}
size_t metadataBytes(char* p,size_t size,size_t count,void* target) {
    auto& out=*static_cast<std::string*>(target);
    if(size && count>(1024*1024-out.size())/size)return 0;
    out.append(p,size*count);return size*count;
}
int cancelled(void*,curl_off_t,curl_off_t,curl_off_t,curl_off_t){return stopping?1:0;}
Release fetchRelease() {
    auto c=request(releaseApi);std::string text;
    curl_easy_setopt(c.get(),CURLOPT_TIMEOUT,30L);
    curl_easy_setopt(c.get(),CURLOPT_WRITEFUNCTION,metadataBytes);
    curl_easy_setopt(c.get(),CURLOPT_WRITEDATA,&text);
    curl_easy_setopt(c.get(),CURLOPT_XFERINFOFUNCTION,cancelled);
    perform(c.get());
    auto j=nlohmann::json::parse(text);
    if(j.value("draft",true)||j.value("prerelease",true))throw std::runtime_error("No stable release available");
    Release r;r.version=j.value("tag_name","");r.title=j.value("name","");r.notes=j.value("body","");
    for(const auto& a:j.at("assets"))if(a.value("name","")=="TsVitch.nro") {
        r.url=a.value("browser_download_url","");r.size=a.value("size",uint64_t{0});
        if(a.contains("digest")&&a["digest"].is_string())r.digest=a["digest"].get<std::string>();
    }
    return r;
}
bool installable(const Release& r) {
    return r.url.rfind("https://github.com/rtk19/TsVitch/releases/download/",0)==0 && r.size>0 && r.size<=maximumSize &&
        r.digest.rfind("sha256:",0)==0 && r.digest.size()==71 &&
        r.digest.find_first_not_of("0123456789abcdef",7)==std::string::npos;
}
std::string megabytes(uint64_t bytes){char s[40];std::snprintf(s,sizeof(s),"%.1f MB",bytes/(1024.*1024.));return s;}
brls::Dialog* blocker(const std::string& text) {
    auto* box=new brls::Box();box->setAxis(brls::Axis::COLUMN);box->setAlignItems(brls::AlignItems::CENTER);
    box->setPadding(28);box->setFocusable(true);box->setHideHighlight(true);
    auto* spinner=new brls::ProgressSpinner(brls::ProgressSpinnerSize::LARGE);
    spinner->setDimensions(48,48);spinner->animate(true);box->addView(spinner);
    auto* label=new brls::Label();label->setText(text);label->setMarginTop(20);box->addView(label);
    auto* d=new brls::Dialog(box);d->setCancelable(false);d->open();return d;
}
bool restartNow() {
#ifdef __SWITCH__
    if(executablePath.empty()||!envHasNextLoad())return false;
    // TsVitch normally exits to HOME. Relaunch requires libnx's loader-return mode.
    brls::Application::getPlatform()->exitToHomeMode(false);
    envSetNextLoad(executablePath.c_str(),executablePath.c_str());
    brls::Application::quit();return true;
#else
    return false;
#endif
}
void restartPrompt() {
    auto* d=new brls::Dialog(tr("update_ready"));d->setCancelable(false);
    d->addButton(tr("s34"),[]{if(!restartNow())note(tr("update_no_relaunch"));});d->open();
}
void prompt(const Release& r);
void changelog(const Release& r) {
    auto* box=new brls::Box();box->setAxis(brls::Axis::COLUMN);box->setWidth(600);box->setPadding(20);
    auto* heading=new brls::Label();heading->setText(r.title.empty()?r.version:r.title);heading->setFontSize(23);heading->setMarginBottom(14);box->addView(heading);
    auto* scroll=new brls::ScrollingFrame();scroll->setHeight(360);scroll->setWidthPercentage(100);scroll->setFocusable(true);
    auto* label=new brls::Label();label->setText(r.notes.empty()?tr("update_no_notes"):r.notes);label->setFontSize(17);label->setSingleLine(false);label->setWidthPercentage(100);
    scroll->setContentView(label);box->addView(scroll);
    auto* d=new brls::Dialog(box);d->setCancelable(false);
    d->addButton(brls::getStr("hints/back"),[r]{prompt(r);});d->open();brls::Application::giveFocus(scroll);
}
struct DownloadUI {
    brls::Dialog* dialog; brls::Label* label; brls::Box* track; brls::Box* fill; bool active=true;
    void progress(uint64_t now,uint64_t total) {
        if(!active)return;
        const auto f=total?std::min(1.f,float(now)/total):0.f;
        label->setText(tr("update_downloading")+" "+std::to_string(int(f*100))+"%\n"+megabytes(now)+" / "+megabytes(total));
        fill->setWidth(track->getWidth()*f);
    }
};
struct Transfer {
    FILE* file=nullptr;uint64_t size=0,expected=0;int lastPercent=-1;
    std::shared_ptr<DownloadUI> ui;
#ifdef __SWITCH__
    Sha256Context hash;
#endif
};
size_t fileBytes(char* p,size_t size,size_t count,void* target) {
    auto& t=*static_cast<Transfer*>(target);
    if(size && count>(t.expected-t.size)/size)return 0;
    const auto bytes=size*count;
    if(std::fwrite(p,1,bytes,t.file)!=bytes)return 0;
#ifdef __SWITCH__
    sha256ContextUpdate(&t.hash,p,bytes);
#endif
    t.size+=bytes;return bytes;
}
int transferProgress(void* target,curl_off_t,curl_off_t,curl_off_t,curl_off_t) {
    if(stopping)return 1;
    auto& t=*static_cast<Transfer*>(target);const int percent=int(t.size*100/t.expected);
    if(percent!=t.lastPercent){t.lastPercent=percent;dispatch([ui=t.ui,now=t.size,total=t.expected]{ui->progress(now,total);});}
    return 0;
}
#ifdef __SWITCH__
std::string hashString(Sha256Context& hash) {
    unsigned char bytes[32];sha256ContextGetHash(&hash,bytes);std::string hex;
    for(auto c:bytes){hex+="0123456789abcdef"[c>>4];hex+="0123456789abcdef"[c&15];}return "sha256:"+hex;
}
std::string fileDigest(const std::string& path) {
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot read staged update");
    Sha256Context hash;sha256ContextCreate(&hash);std::array<char,32768> block;
    while(file){file.read(block.data(),block.size());sha256ContextUpdate(&hash,block.data(),file.gcount());}
    if(!file.eof())throw std::runtime_error("Cannot verify staged update");return hashString(hash);
}
#endif
void downloadRelease(const Release& r,std::shared_ptr<DownloadUI> ui) {
    const auto part=executablePath+".part", staged=executablePath+".new", receipt=executablePath+".update.json";
    Transfer t;t.expected=r.size;t.ui=ui;
    try {
        t.file=std::fopen(part.c_str(),"wb");if(!t.file)throw std::runtime_error("Cannot write update. Check SD card space.");
#ifdef __SWITCH__
        sha256ContextCreate(&t.hash);
#endif
        auto c=request(r.url);curl_easy_setopt(c.get(),CURLOPT_TIMEOUT,600L);
        curl_easy_setopt(c.get(),CURLOPT_MAXFILESIZE_LARGE,static_cast<curl_off_t>(r.size));
        curl_easy_setopt(c.get(),CURLOPT_WRITEFUNCTION,fileBytes);curl_easy_setopt(c.get(),CURLOPT_WRITEDATA,&t);
        curl_easy_setopt(c.get(),CURLOPT_XFERINFOFUNCTION,transferProgress);curl_easy_setopt(c.get(),CURLOPT_XFERINFODATA,&t);
        perform(c.get());
        const bool flushed=std::fflush(t.file)==0;const bool closed=std::fclose(t.file)==0;t.file=nullptr;
        if(!flushed||!closed)throw std::runtime_error("Cannot finish writing update. Check SD card space.");
        dispatch([ui]{if(ui->active)ui->label->setText(tr("update_verifying"));});
        if(t.size!=r.size||!update::validNro(part,r.size))throw std::runtime_error("Invalid or incomplete NRO");
#ifdef __SWITCH__
        if(hashString(t.hash)!=r.digest || fileDigest(part)!=r.digest)throw std::runtime_error("Update checksum mismatch");
#endif
        if(stopping)throw std::runtime_error("Update cancelled");
        std::filesystem::rename(part,staged);
        std::ofstream out(receipt,std::ios::trunc);out<<nlohmann::json{{"version",r.version},{"size",r.size},{"digest",r.digest}};out.flush();
        if(!out.good()){std::filesystem::remove(staged);throw std::runtime_error("Cannot save update verification");}
        out.close();if(out.fail()){std::filesystem::remove(staged);throw std::runtime_error("Cannot close update verification");}
        pending=true;
#ifdef __SWITCH__
        fsdevCommitDevice("sdmc");
#endif
        dispatch([ui]{ui->active=false;busy=false;ui->dialog->close([]{restartPrompt();});});
    } catch(const std::exception& e) {
        if(t.file)std::fclose(t.file);
        std::error_code ec;std::filesystem::remove(part,ec);
        const std::string error=e.what();dispatch([ui,error]{ui->active=false;busy=false;ui->dialog->close([error]{note(tr("update_failed")+"\n"+error);});});
    }
}
void beginDownload(const Release& r) {
#ifndef __SWITCH__
    note("In-app installation is available on Nintendo Switch");return;
#else
    if(executablePath.rfind("sdmc:/",0)!=0 || std::filesystem::path(executablePath).extension()!=".nro") {
        note(tr("s38"));return;
    }
#endif
    if(busy.exchange(true))return;
    auto ui=std::make_shared<DownloadUI>();
    auto* box=new brls::Box();box->setAxis(brls::Axis::COLUMN);box->setAlignItems(brls::AlignItems::CENTER);box->setPadding(28,32,28,32);box->setFocusable(true);box->setHideHighlight(true);
    ui->label=new brls::Label();ui->label->setText(tr("update_downloading")+" 0%");ui->label->setFontSize(20);ui->label->setSingleLine(false);ui->label->setHorizontalAlign(brls::HorizontalAlign::CENTER);box->addView(ui->label);
    ui->track=new brls::Box();ui->track->setDimensions(320,8);ui->track->setShrink(0);ui->track->setCornerRadius(4);ui->track->setMarginTop(18);ui->track->setBackgroundColor(nvgRGBA(128,128,128,80));
    ui->fill=new brls::Box();ui->fill->setDimensions(0,8);ui->fill->setCornerRadius(4);ui->fill->setBackgroundColor(brls::Application::getTheme().getColor("brls/accent"));ui->track->addView(ui->fill);box->addView(ui->track);
    ui->dialog=new brls::Dialog(box);ui->dialog->setCancelable(false);ui->dialog->open();
    start([r,ui]{downloadRelease(r,ui);});
}
void prompt(const Release& r) {
    auto* d=new brls::Dialog(tr("update_available")+" "+r.version+" ("+megabytes(r.size)+")\n\n"+r.title);
    d->addButton(tr("s35"),[]{});d->addButton(tr("update_changelog"),[r]{changelog(r);});d->addButton(tr("update_install"),[r]{beginDownload(r);});
    if(auto* button=d->getView("brls/dialog/button3"))d->setLastFocusedView(button);d->open();
}
bool recoverPending() {
#ifdef __SWITCH__
    const auto receipt=executablePath+".update.json", staged=executablePath+".new";
    if(!std::filesystem::exists(receipt)||!std::filesystem::exists(staged))return false;
    std::ifstream f(receipt);nlohmann::json j;f>>j;
    const auto size=j.at("size").get<uint64_t>();
    if(size>maximumSize||!update::validNro(staged,size)||fileDigest(staged)!=j.at("digest").get<std::string>()) {
        std::error_code ec;std::filesystem::remove(staged,ec);std::filesystem::remove(receipt,ec);
        throw std::runtime_error("Staged update verification failed. Check for updates again.");
    }
    pending=true;return true;
#else
    return false;
#endif
}
}
void setApplicationPath(const std::string& path) {
    executablePath=path;
#ifdef __SWITCH__
    if(!path.empty()&&path.front()=='/')executablePath="sdmc:"+path;
#endif
}
void checkForUpdates(bool showUpToDate) {
    if(busy.exchange(true))return;
    auto* loading=showUpToDate?blocker(tr("s33")):nullptr;
    start([showUpToDate,loading]{
        try {
            const auto errorPath=executablePath+".update-error.txt";
            if(!executablePath.empty()&&std::filesystem::exists(errorPath)) {
                std::ifstream input(errorPath);std::string error((std::istreambuf_iterator<char>(input)),{});
                input.close();std::filesystem::remove(errorPath);
                dispatch([loading,error]{busy=false;auto show=[error]{note(tr("update_failed")+"\n"+error);};if(loading)loading->close(show);else show();});return;
            }
            if(recoverPending()) {dispatch([loading]{busy=false;if(loading)loading->close([]{restartPrompt();});else restartPrompt();});return;}
            const auto r=fetchRelease();auto& v=APPVersion::instance();
            const bool newer=isNewerStableRelease(r.version,{v.major,v.minor,v.revision});
            if(newer&&!installable(r))throw std::runtime_error("Release has no verified TsVitch.nro asset");
            dispatch([r,newer,showUpToDate,loading]{
                busy=false;auto result=[r,newer,showUpToDate]{if(newer)prompt(r);else if(showUpToDate)note(tr("s32")+" ("+APPVersion::instance().getVersionStr()+")");};
                if(loading)loading->close(result);else result();
            });
        } catch(const std::exception& e) {
            const std::string error=e.what();dispatch([loading,showUpToDate,error]{busy=false;auto fail=[showUpToDate,error]{if(showUpToDate)note(tr("update_check_failed")+"\n"+error);};if(loading)loading->close(fail);else fail();});
        }
    });
}
void installLatestUpdate(){checkForUpdates(true);}
void shutdownUpdater(){stopping=true;if(worker.joinable())worker.join();}
void applyPendingUpdate() {
#ifdef __SWITCH__
    if(!pending||executablePath.empty())return;
    // No resource reads after this point: hbloader holds the NRO through RomFS.
    romfsExit();std::string error;
    if(update::replaceNro(executablePath,error)) {
        std::remove((executablePath+".update.json").c_str());
    } else {
        std::ofstream out(executablePath+".update-error.txt");out<<error;
    }
    fsdevCommitDevice("sdmc");
#endif
}
}
