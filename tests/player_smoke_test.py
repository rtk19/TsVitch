"""Real libmpv playback: embedded audio/subtitles, player UI, EPG and non-Range catch-up."""
import base64
import datetime
import http.server
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import urllib.parse

binary=Path(sys.argv[1]).resolve()
repo=Path(__file__).resolve().parents[1]
(repo/'cmake-smoke').mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='tsvitch-player-') as directory:
    work=Path(directory);(work/'resources').symlink_to(repo/'resources',target_is_directory=True)
    for lang,text in [('en','English subtitle'),('he','שלום עולם · עברית 123')]:
        (work/f'{lang}.srt').write_text(''.join(f'{i+1}\n00:{(i*2)//60:02d}:{(i*2)%60:02d},000 --> 00:{(i*2+2)//60:02d}:{(i*2+2)%60:02d},000\n{text}\n\n' for i in range(60)),encoding='utf-8')
    subprocess.run(['ffmpeg','-v','error','-f','lavfi','-i','testsrc2=s=320x180:r=15:d=120','-f','lavfi','-i','sine=frequency=440:duration=120','-f','lavfi','-i','sine=frequency=660:duration=120','-i',str(work/'en.srt'),'-i',str(work/'he.srt'),'-map','0:v','-map','1:a','-map','2:a','-map','3:s','-map','4:s','-c:v','mpeg4','-q:v','9','-c:a','aac','-b:a','32k','-c:s','srt','-metadata:s:a:0','language=eng','-metadata:s:a:1','language=heb','-metadata:s:s:0','language=eng','-metadata:s:s:1','language=heb','-t','120',str(work/'tracks.mkv')],check=True)
    subprocess.run(['ffmpeg','-v','error','-i',str(work/'tracks.mkv'),'-t','2','-map','0:v','-map','0:a:0','-c','copy',str(work/'short.mkv')],check=True)
    for i in range(3):
        subprocess.run(['ffmpeg','-v','error','-f','lavfi','-i',f'testsrc2=s=160x90:r=10:d=60','-f','lavfi','-i','sine=frequency=440:duration=60','-c:v','mpeg2video','-g','10','-q:v','9','-c:a','mp2','-b:a','64k','-f','mpegts',str(work/f'{i}.ts')],check=True)
    import shutil
    shutil.copy2(work/'tracks.mkv',repo/'cmake-smoke/tracks.mkv')
    archive_start=int(time.time()-3600)//60*60
    archive_requests=[]
    class Provider(http.server.BaseHTTPRequestHandler):
        def log_message(self,*args):pass
        def do_GET(self):
            path=urllib.parse.urlsplit(self.path);params=urllib.parse.parse_qs(path.query);action=params.get('action',[''])[0]
            if path.path.startswith('/timeshift/'):
                date=path.path.split('/')[-2];start=int(datetime.datetime.strptime(date,'%Y-%m-%d:%H-%M').replace(tzinfo=datetime.timezone.utc).timestamp());segment=(start-archive_start)//60
                archive_requests.append(segment);body=(work/f'{min(2,max(0,segment))}.ts').read_bytes();content='video/mp2t'
            elif path.path.startswith('/series/'):
                body=(work/'short.mkv').read_bytes();content='video/x-matroska'
            elif path.path.startswith('/live/'):
                body=(work/'tracks.mkv').read_bytes();content='video/x-matroska'
            else:
                now=int(time.time())
                programmes=[{'title':base64.b64encode(title.encode()).decode(),'description':'Programme fixture','start_timestamp':now+start,'stop_timestamp':now+end,'now_playing':flag} for title,start,end,flag in [('Past programme',-10800,-7200,0),('Wrong device-clock match',-300,300,0),('השידור הנכון לפי הספק',-7200,-3600,1),('Later programme',3600,7200,0)]]
                if action=='get_live_streams':data=[{'stream_id':i+1,'name':f'Channel {i}','category_id':'1','tv_archive':1,'tv_archive_duration':7} for i in range(50)]
                elif action=='get_live_categories':data=[{'category_id':'1','category_name':'Fixture'}]
                elif action in ('get_short_epg','get_simple_data_table'):data={'epg_listings':programmes}
                else:data={'user_info':{'auth':1,'status':'Active'},'server_info':{'timestamp_now':now,'timezone':'UTC'}}
                body=json.dumps(data).encode();content='application/json'
            self.send_response(200);self.send_header('Content-Type',content);self.send_header('Content-Length',str(len(body)));self.send_header('Accept-Ranges','none');self.end_headers()
            try:self.wfile.write(body)
            except (BrokenPipeError,ConnectionResetError):pass
    server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Provider);threading.Thread(target=server.serve_forever,daemon=True).start()
    config=work/'config/tsvitch';config.mkdir(parents=True);url=f'http://127.0.0.1:{server.server_port}'
    (config/'tsvitch_config.json').write_text(json.dumps({'setting':{'app_lang':'he','iptv_mode':1,'xtream_server_url':url,'xtream_username':'fixture','xtream_password':'fixture','xtream_enabled':1,'home_window_state':'0,1280x720,0x0'},'client':'fixture','device':'fixture'}))
    env=os.environ.copy();env['NO_PROXY']=env['no_proxy']='127.0.0.1,localhost';env['TSVITCH_ARCHIVE_START']=str(archive_start);env['TSVITCH_PLAYER_FIXTURE']='1'
    log=work/'player.log'
    finished=threading.Event()
    if env.get('TSVITCH_UI_CAPTURE'):
        capture=Path(env['TSVITCH_UI_CAPTURE']);capture.mkdir(parents=True,exist_ok=True)
        def snapshots():
            seen=set()
            while not finished.wait(.03):
                try:stage=int((capture/'stage').read_text())
                except (OSError,ValueError):continue
                if stage not in seen:
                    seen.add(stage);subprocess.run(['scrot','-o',str(capture/f'{stage}.png')],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        threading.Thread(target=snapshots,daemon=True).start()
    try:
        process=subprocess.Popen([str(binary),'--player-smoke-test','-d','-o',str(log)],cwd=work,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        (repo/'cmake-smoke/player.pid').write_text(str(process.pid))
        if env.get('TSVITCH_PLAYER_DEBUG'):
            def debug_stack():
                time.sleep(12)
                if process.poll() is None:
                    with (repo/'cmake-smoke/player-gdb.log').open('w') as trace:
                        subprocess.run(['gdb','-batch','-ex','set pagination off','-p',str(process.pid),'-ex','thread apply all bt 14'],stdout=trace,stderr=subprocess.STDOUT)
            threading.Thread(target=debug_stack,daemon=True).start()
        try:output,_=process.communicate(timeout=80)
        except subprocess.TimeoutExpired:
            process.kill();process.communicate();raise
        result=subprocess.CompletedProcess(process.args,process.returncode,stdout=output)
        trace=log.read_text(errors='replace') if log.exists() else ''
        assert result.returncode==0,(result.returncode,result.stdout[-4000:],trace[-6000:])
        assert 'PLAYER_SMOKE_PASSED' in trace,trace[-6000:]
        assert 0 in archive_requests and 1 in archive_requests,archive_requests
        print('Player passed: Hebrew audio/subtitles, delay/background, EPG current marker, channel focus after next/close, catch-up forward/back/rapid seeks, pause/speed preservation and next-episode autoplay',flush=True)
    except Exception:
        import shutil
        shutil.copy2(work/'tracks.mkv',repo/'cmake-smoke/tracks.mkv')
        if log.exists():(repo/'cmake-smoke/player-failure.log').write_text(log.read_text(errors='replace'))
        print(log.read_text(errors='replace')[-9000:] if log.exists() else 'No player log',file=sys.stderr)
        raise
    finally:finished.set();server.shutdown();server.server_close()
