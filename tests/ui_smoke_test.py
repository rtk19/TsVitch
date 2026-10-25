"""Run the real desktop UI against a local IPTV fixture, with an isolated profile."""
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
class Provider(http.server.BaseHTTPRequestHandler):
    def log_message(self,*args): pass
    def do_GET(self):
        action=urllib.parse.parse_qs(urllib.parse.urlsplit(self.path).query).get('action',[''])[0]
        if self.path.startswith('/playlist'):
            body=('#EXTM3U\n'+''.join(f'#EXTINF:-1 tvg-id="ch{i}" group-title="Group {i%10}",Channel {i}\nhttp://127.0.0.1/live/{i}.ts\n' for i in range(500))+'#EXTINF:-1 type="movie" group-title="Movies",Test Movie\nhttp://127.0.0.1/movie/1.mp4\n').encode()
        else:
            data={
                'get_live_categories':[{'category_id':str(i),'category_name':f'Group {i}'} for i in range(10)],
                'get_live_streams':[{'stream_id':i+1,'num':i+1,'name':f'Channel {i}','category_id':str(i%10),'tv_archive':i%2,'tv_archive_duration':7} for i in range(500)],
                'get_vod_categories':[{'category_id':'1','category_name':'Movies'}],
                'get_series_categories':[{'category_id':'1','category_name':'Series'}],
                'get_vod_streams':[{'stream_id':1,'name':'Test Movie','category_id':'1','container_extension':'mp4'}],
                'get_series':[{'series_id':1,'name':'Test Series','category_id':'1'}],
                'get_short_epg':{'epg_listings':[]},
            }.get(action,{'user_info':{'status':'Active','auth':1}})
            body=json.dumps(data).encode()
        self.send_response(200);self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Provider)
threading.Thread(target=server.serve_forever,daemon=True).start()
url=f'http://127.0.0.1:{server.server_port}'
try:
    for mode,locale in [(0,'en-US'),(1,'he')]:
        with tempfile.TemporaryDirectory(prefix='tsvitch-ui-') as folder:
            work=Path(folder);(work/'resources').symlink_to(resources,target_is_directory=True)
            config=work/'config/tsvitch';config.mkdir(parents=True)
            settings={'app_lang':locale,'iptv_mode':mode,'m3u8_url':url+'/playlist.m3u','xtream_server_url':url,'xtream_username':'fixture','xtream_password':'fixture','xtream_enabled':1}
            (config/'tsvitch_config.json').write_text(json.dumps({'setting':settings,'client':'fixture','device':'fixture'}))
            env=os.environ.copy();env['NO_PROXY']='localhost,127.0.0.1';env['no_proxy']=env['NO_PROXY']
            log=work/'ui.log'
            try:
                result=subprocess.run([str(binary),'--ui-smoke-test','-d','-o',str(log)],cwd=work,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
                trace=log.read_text(errors='replace') if log.exists() else ''
                assert result.returncode==0,(result.returncode,result.stdout[-4000:],trace[-12000:])
                assert 'UI_SMOKE_PASSED' in trace,trace[-12000:]
                assert 'received 500 channels' in trace,trace[-12000:]
                assert 'Fatal error:' not in trace,trace[-12000:]
                print(f'UI smoke passed: {locale}, {"Xtream" if mode else "M3U"}, 500 channels, VOD focus, nested grids, Settings')
            except Exception:
                print(log.read_text(errors='replace')[-16000:] if log.exists() else 'No startup log',file=sys.stderr)
                raise
finally:
    server.shutdown();server.server_close()
