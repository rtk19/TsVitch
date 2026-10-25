"""Exercise real UI actions and count provider requests across memory/disk cache hits."""
import collections
import http.server
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import urllib.parse

binary=Path(sys.argv[1]).resolve()
resources=Path(__file__).resolve().parents[1]/'resources'
counts=collections.Counter()
count_lock=threading.Lock()
MOVIES=4000
SERIES=1000
playlist=('#EXTM3U\n'+''.join(f'#EXTINF:-1 tvg-id="ch{i}" group-title="Group {i%10}",Channel {i}\nhttp://127.0.0.1/live/{i}.ts\n' for i in range(500))+
    ''.join(f'#EXTINF:-1 type="movie" tvg-id="movie_{i}" group-title="Movies",Movie {i:04d}\nhttp://127.0.0.1/movie/fixture/p/{i}.mp4\n' for i in range(MOVIES))+
    ''.join(f'#EXTINF:-1 type="series" tvg-id="series_{i}_{episode}" series-title="Series {i:04d}" season="1" episode="{episode}" group-title="Series",Series {i:04d} E{episode}\nhttp://127.0.0.1/series/fixture/p/{i}_{episode}.mkv\n' for i in range(SERIES) for episode in (1,2))).encode()
class Provider(http.server.BaseHTTPRequestHandler):
    def log_message(self,*args): pass
    def do_GET(self):
        params=urllib.parse.parse_qs(urllib.parse.urlsplit(self.path).query)
        action=params.get('action',[''])[0]
        counter='playlist' if self.path.startswith('/playlist') else action
        with count_lock: counts[counter]+=1
        if counter=='playlist': body=playlist
        else:
            if action=='get_series_info':
                series=params.get('series_id',['1'])[0]
                data={'episodes':{'1':[{'id':int(series)*100+i,'title':f'Episode {i}','season':1,'episode_num':i,'container_extension':'mkv'} for i in (1,2)],'2':[{'id':int(series)*100+3,'title':'Episode 3','season':2,'episode_num':1,'container_extension':'mkv'}]}}
            else:
                data={
                    'get_live_categories':[{'category_id':str(i),'category_name':f'Group {i}'} for i in range(10)],
                    'get_live_streams':[{'stream_id':i+1,'num':i+1,'name':f'Channel {i}','category_id':str(i%10),'tv_archive':i%2,'tv_archive_duration':7} for i in range(500)],
                    'get_vod_categories':[{'category_id':'1','category_name':'Movies'}],
                    'get_series_categories':[{'category_id':'1','category_name':'Series'}],
                    'get_vod_streams':[{'stream_id':i+1,'name':f'Movie {i:04d}','category_id':'1','container_extension':'mp4'} for i in range(MOVIES)],
                    'get_series':[{'series_id':i+1,'name':f'Series {i:04d}','category_id':'1'} for i in range(SERIES)],
                    'get_short_epg':{'epg_listings':[]},
                }.get(action,{'user_info':{'status':'Active','auth':1}})
            body=json.dumps(data).encode()
        self.send_response(200);self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Provider)
threading.Thread(target=server.serve_forever,daemon=True).start()
url=f'http://127.0.0.1:{server.server_port}'
try:
    for mode,locale in [(0,'en-US'),(1,'he')]:
        with count_lock:counts.clear()
        with tempfile.TemporaryDirectory(prefix='tsvitch-ui-') as folder:
            work=Path(folder);(work/'resources').symlink_to(resources,target_is_directory=True)
            config=work/'config/tsvitch';config.mkdir(parents=True)
            settings={'app_lang':locale,'iptv_mode':mode,'m3u8_url':url+'/playlist.m3u','xtream_server_url':url,'xtream_username':'fixture','xtream_password':'fixture','xtream_enabled':1}
            (config/'tsvitch_config.json').write_text(json.dumps({'setting':settings,'client':'fixture','device':'fixture'}))
            env=os.environ.copy();env['NO_PROXY']='localhost,127.0.0.1';env['no_proxy']=env['NO_PROXY']
            def run(flag):
                log=work/'ui.log'
                try:
                    result=subprocess.run([str(binary),flag,'-d','-o',str(log)],cwd=work,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=65)
                    trace=log.read_text(errors='replace') if log.exists() else ''
                    assert result.returncode==0,(result.returncode,result.stdout[-2000:],trace[-5000:])
                    assert 'UI_SMOKE_PASSED' in trace,trace[-5000:]
                    assert 'received 500 channels' in trace,trace[-5000:]
                    assert 'Fatal error:' not in trace,trace[-5000:]
                except Exception:
                    print(log.read_text(errors='replace')[-6000:] if log.exists() else 'No startup log',file=sys.stderr)
                    raise
            run('--ui-smoke-test')
            with count_lock:first=counts.copy()
            keys=['playlist'] if mode==0 else ['get_vod_streams','get_vod_categories','get_series','get_series_categories','get_series_info']
            expected={k:(2 if k in ("playlist","get_vod_streams","get_vod_categories") else 1) for k in keys}
            assert all(first[k]==expected[k] for k in keys),(dict(first),expected)
            run('--ui-smoke-cache')
            with count_lock:second=counts.copy()
            assert all(second[k]==first[k] for k in keys),(dict(first),dict(second))
            print(f'UI smoke passed: {locale}, {"Xtream" if mode else "M3U"}, 500 channels, {MOVIES} movies, {SERIES} series; pin/favorite actions; no extra catalogue or episode requests on reopen/restart',flush=True)
finally:
    server.shutdown();server.server_close()
