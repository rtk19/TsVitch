"""Exercise real UI actions and count provider requests across memory/disk cache hits."""
import base64
import time
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
PNG='iVBORw0KGgoAAAANSUhEUgAAAWgAAAH0CAIAAACfIyC6AAAPMUlEQVR4nO3ce4yldX3H8d/uDroLBLCNBKRUuYnQSkEaraYtEMU2EQoWTSgxZq30FkBIFJMSIeUi/mGwLUK0iqVaYrHlJtAaXCsXI1e5LFB2l10XV9kiprdNw0XYS/8YMh3Ozs7O55zznOf2ev0FM3Oe8/D8fvN+vs/Msov2PvGMApBYXPcJAO0jHEBMOICYcAAx4QBiwgHEhAOICQcQEw4gJhxATDiAmHAAMeEAYsIBxIQDiAkHEBMOICYcQEw4gJhwADHhAGLCAcSEA4gJBxATDiAmHEBMOICYcAAx4QBiwgHEhAOICQcQEw4gJhxAbKqKg6772ItVHBYYzsGXLx3vAU0cQEw4gJhwADHhAGLCAcSEA4gJBxATDiAmHEBMOICYcAAx4QBiwgHEhAOICQcQEw4gJhxATDiAmHAAMeEAYsIBxIQDiAkHEBMOICYcQEw4gJhwADHhAGLCAcSEA4gJBxATDiAmHEBMOICYcAAx4QBiwgHEhAOICQcQEw4gJhxATDiAmHAAMeEAYsIBxIQDiAkHEBMOICYcQEw4gJhwADHhAGLCAcSEA4gJBxATDiAmHEBMOICYcAAx4QBiwgHEhAOICQcQEw4gJhxATDiAmHAAMeEAYsIBxIQDiAkHEBMOICYcQEw4gJhwADHhAGLCAcSEA4gJBxATDiAmHEBMOICYcAAx4QBiwgHEhAOICQcQEw4gJhxATDiAmHAAMeEAYsIBxIQDiE3VfQLU7Nf+as/hXrjynE3jPRNaRDh6ZOhGLPxoatITwtFl4y3FEO+oI10lHF0z+VjMY/bJiEiXCEdHNKoXc5o5QwXpAOFot+b3YnsK0gHC0Upt7MX2FKS9hKNlupGMAdP/UfLRIsLRDp3sxQADSIsIR9P1IRkDDCDNJxzN1cNkzCYfTSYcTdTzZMwmH80kHM0iGXOSj6bxf8c2iGrMz/VpDhNHI/iWWCCjR0MIR80kYwjyUTuPKnVSjVG4ejUSjtrY96NzDeviUaUGtvsYeWyphYlj0lSjCq7qhAnHRNnf1XFtJ8mjyoTY1hPgsWViTByToBqT5GpPgHBUzj6ePNe8asJRLTu4Lq58pYSjQvZuvVz/6ghHVezaJrAKFRGOStivzWEtqiAc42enNo0VGTvhGDN7tJmsy3gJxzjZnU1mdcZIOMbGvmw+azQuwjEedmRbWKmxEI4xsBfbxXqNTjhGZRe2kVUbkXCMxP5rL2s3CuEAYsIxPLestrOCQxOOIdlz3WAdhyMcw7DbusRqDkE4umb9rZfVfQp0n3DEmn+D0o5U89e0aYQj05Ydtv7Wy+Qj0paVbQjhCLRub2lHpHXrWyPh6DijB1UQjoVq9e1IOxao1as8ScLRF0YPxkg4FqQzNyLt2KnOrHWlhGPnOraTjB471bEVr4Jw9JR2MArh2IkO33yMHvPo8LqPhXD0nXYwBOGYT09uO9oxp56s/nCEg1I8thASjh3q4Q1HOwb0cA8skHDwKkYPFkI45tbzW412zOj5TtgR4WBuRg/mIRzMRzuYk3DMwXQ6m9HDftiecLAgPW8HA4RjkNvLjvR59LArBggHmd62g9mEg5h2IByvYiJdoB4+ttgbswkHw+tbO5ghHIykh6MHRThmM4sOrSftsENmCAfjYfToFeFgnLSjJ4TjFabQcen26GGfTBMOKtHhdlCEg+p0e/ToOeGgWtrRScJRigfXinWsHXZLEQ4mw2NLxwgHk6MdnSEcTJTRoxuEgxpoR9sJh5911aPVo4c9IxzUqb3t6DnhoGatHj16SzhoBO1oF+GgKYweLSIcNIt2tIJw0Dja0Xx9D4ffqzVT8x9ber5z+h4Omqzh7egz4aDRmj969JNw0ALa0TTCQTsYPRpFOGgT7WgI4aBljB5NIBy0knbUSzhoK6NHjYSDdtOOWggH7XbgCR+v+xT6aKruE4AhSUaNTBy0kmrUy8RBy0hGE5g4aBPVaAgTB+0gGY1i4qAFVKNpTBw0mmQ0k4mD5lKNxur7xLHynE09/zvgmqn5yVh5zqa6T6FOJg4ap/nVQDhoFtVohb4/qtAcktEiJg4aQTXaxcRBzSSjjUwc1Ek1WsrE4Tey9Wh1Mnr+u9hi4qAWra4GxcTBhElGN5g4mBzV6AwTB5MgGR1j4ijFz7oq1rFq2C1FOKhax6rBNI8qVEUyOszEQSVUo9tMHK/wx8DGpdvJ8AOOaSYOxqnb1WCGiYPxkIxeMXH8P1Po0HpSDTtkhomDkfQkGQwwcTA81egtE8er+N3KAvUwGZ5TZjNxEOthNRggHGRUg+JRZXueVnakz8nwnDLAxMGC9LkabM/EMQdDx2ySYdzYnomD+agGczJxMDfJYB4mjrn1fDpVjRk93wk7YuLgVSSDhTBx7FAPbzWqMaCHe2CBTByUIhmETBzz6ckNRzXm1JPVH45w9J1qMASPKjvR4T8MJhnzMG7Mz8TRU6rBKEwcO9exoUMydsq4sVMmjgXpzE5SjZ3qzFpXysTRF5LBGJk4FqrVNyLVWKBWr/IkmTg6TjKogokj0LrbkWpEWre+NTJxZNryGxbJSKlGxMQRa/4OU41U89e0aYSja1SDCRCOYbhBdYnVHIJwDMlu6wbrOBzhGJ4913ZWcGjCAcSEYyRuWe1l7UYhHKOy/9rIqo1IOMbALmwX6zU64RgPe7EtrNRYCMfY2JHNZ43GRTjGyb5sMqszRsIxZnZnM1mX8RKO8bNHm8aKjJ1wVMJObQ5rUQXhqIr92gRWoSLCUSG7tl6uf3WEo1r2bl1c+UoJR+Xs4MlzzasmHJNgH0+Sqz0B/rLiCZneza34i47bSzImxsQxUXZ2dVzbSRKOSbO/q+CqTphHlRp4bBkjyaiFiaM2dvzoXMO6CEed7PtRuHo18qhSM48tQ5CM2glHI8jHAklGQ3hUaRDfFfNzfZrDxNEsRo85SUbTCEcTyccMyWgm4WiunudDMppMOJquh/mQjOYTjnaY+V7qcEH0okWEo2U6OYBIRusIRyt1YwDRi/YSjnZrY0H0ogOEoyOaXxC96BLh6JrZ35+1R0Qsuko4umzg+3YCHVGKnhCOHpnzu3rommhEnwlH3/n+Zwj+71ggJhxATDiAmHAAMeEAYsIBxIQDiAkHEBMOICYcQEw4gJhwADHhAGLCAcSEA4gJBxATDiAmHEBMOICYcAAx4QBiwgHEhAOICQcQEw4gJhxATDiAmHAAMeEAYsIBxIQDiAkHEBMOICYcQEw4gJhwADHhAGLCAcSEA4gJBxATDiAmHEBMOICYcAAx4QBiwgHEhAOICQcQEw4gJhxATDiAmHAAMeEAYsIBxIQDiAkHEBMOICYcQEw4gJhwADHhAGLCAcSEA4gJBxATDiAmHEBMOICYcAAx4QBiwgHEhAOICQcQEw4gJhxATDiAmHAAMeEAYsIBxIQDiAkHEBMOICYcQEw4gJhwADHhAGLCAcSEA4gJBxATDiAmHEBMOICYcAAx4QBiwgHEhAOICQcQEw4gJhxATDiA2FQVBz348qVVHBZoCBMHEBMOICYcQEw4gJhwADHhAGLCAcSEA4gJBxATDiAmHEBMOICYcAAx4QBiwgHEhAOICQcQEw4gJhxATDiAmHAAMeEAYsIBxIQDiAkHEBMOICYcQEw4gJhwADHhAGJTdZ/ApF2w/OR1G5/9+op7pv/1Hy8688KrbzzioP3/8H3HvLx58y5TU1fdcsc3vntfKWXDdX/50JM/KtvKLrssuea2u6/913sXcvxXXlVKKeW2+x/74k3fXXvtZw859dzj3nbYh3/nNz/ymS+XUg795X0vP/tDn79+xUdPOKaU8o7DD7rviR+WUv72n+/63FmnHXLqudMvn37hzDEXLVq0+7KlF1x1/d2Pr93+XcZ2gWABeheOFQ88fvqJx06HY7elr93v9a/b+3V7nHb8uz7wqcs3PffCnrstu+aCP3vmP//nrpVrXtq8+f3n/XUpZdelr/n7T/3p8y/+/ObvP7zT48+8asDtD6366AnHvPNXDr7n39ZdfPop533pugfXPHXr3Y+UUtZe+9mZl3zurNPmOeZhb3zDFz6x/NizLt3Ru8Bk9O5R5f5V69964C9NLVlcSvntIw+9/aFVZ7z/PRdefeOm514opWx67oWLrr7prA+8d/ZLnn/xpYv+7qY/+r3jRnzrv/jKjecvP+mEdx359M/+68E1Tw1xhNU/fmbfX9hrxNOA0fUuHFu2bn1wzY+OPvSAUsrxv/6r37r30UP23+ex9U/PfMGj63/y5v33GXjVE09tPGDf14/41us2PvvQkxsuPv2Ui7/6zeGOcOxRb/neo2tGPA0YXe8eVUopt93/2LuPPvy+J3549KFv+uQXrh347KJStm3bNvDBJUsWv7x5y+yP/PmHTnz74Qd++ZY7/uWelbM//pqpqRsvPXv6n8+98tp1G5+d/dndly3dvGXrbste+9//+9zCT3j6mLtMLTlkv31+68xLBt7l01+7+Qerh5lfYGh9DMftDz3xJycd99bvP/zY+qc3b9n65E9+esSB+z+wev30Z484aP81P35m4CVve/ObVm3499kf+cw1t8x58Hl++vD2ww7cY7eln7jyHy794w9++JK/mfNrtm7dtmTx4i1bt04tWbxly9aBY575++859d2/cfl13/YzDurVu0eVUsqm51544ecv/cHx7/zWvY+WUq684TsXfOTkPXZdVkrZc7dl5y8/+YobvjP76/fafdfzl590xQ0rRnnTqSWLLz79lAuvvunOR1Zv3rLld99xxJxf9sjaDccc+ZZSynFHHf7w2g0Dn73zkdVHHfLGUU4DxqKPE0cp5dsPPP7J0953yVe/WUq54+FVb/jFvW649OyXXn55l6mpr9x65/dWrikzjwPbytTUks9fv+Lux9aO8o6nn3jsnSvXbPjpf5RSzr/q+m9ceOZdK1c//+JLA1923pf+6bIzT/vYB99bSvn4FV8f+Oy6jT87/ID9Fi9aNPtR5Qern/r0124e5dwgtWjvE8+o+xyAlunjowowIuEAYsIBxIQDiAkHEBMOICYcQEw4gJhwADHhAGLCAcSEA4gJBxATDiAmHEBMOICYcAAx4QBiwgHEhAOICQcQEw4gJhxATDiAmHAAMeEAYsIBxIQDiAkHEBMOICYcQEw4gJhwADHhAGLCAcSEA4gJBxATDiAmHEDs/wC8JfoInEuafQAAAABJRU5ErkJggg=='
WEBP='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'
MOVIES=4000
SERIES=1000
playlist=('#EXTM3U x-tvg-url="/guide.xml"\n'+''.join(f'#EXTINF:-1 tvg-id="ch{i}" tvg-logo="/art/logo.svg" group-title="Group {i%10}",Channel {i}\nhttp://127.0.0.1/live/{i}.ts\n' for i in range(500))+
    ''.join(f'#EXTINF:-1 type="movie" tvg-id="movie_{i}" tvg-logo="/art/poster.webp?size=large" group-title="Movies",Movie {i:04d}{" · שלום" if i==0 else ""}\nhttp://127.0.0.1/movie/fixture/p/{i}.mp4\n' for i in range(MOVIES))+
    ''.join(f'#EXTINF:-1 type="series" tvg-id="series_{i}_{episode}" series-title="Series {i:04d}" season="1" episode="{episode}" tvg-logo="/art/poster.png" group-title="Series",Series {i:04d} E{episode}\nhttp://127.0.0.1/series/fixture/p/{i}_{episode}.mkv\n' for i in range(SERIES) for episode in (1,2))+''.join(f'#EXTINF:-1 type="series" tvg-id="series_{i}_alt" series-title="Series {i:04d}" season="1" episode="1" group-title="Series",חלופה לפרק 1\nhttp://127.0.0.1/series/fixture/p/{i}_alt.mkv\n' for i in range(SERIES))).encode()
class Provider(http.server.BaseHTTPRequestHandler):
    def log_message(self,*args): pass
    def do_GET(self):
        params=urllib.parse.parse_qs(urllib.parse.urlsplit(self.path).query)
        action=params.get('action',[''])[0]
        counter='playlist' if self.path.startswith('/playlist') else action
        with count_lock: counts[counter]+=1
        if self.path.startswith('/art/'):
            body=(b'<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100"><rect width="100" height="100" rx="12" fill="#efc85a"/><path d="M30 25 L30 75 L75 50 Z" fill="#214775"/></svg>' if '.svg' in self.path else base64.b64decode(WEBP if '.webp' in self.path else PNG))
        elif self.path.startswith('/guide.xml'):
            start=time.strftime('%Y%m%d%H%M%S +0000',time.gmtime(time.time()-600));end=time.strftime('%Y%m%d%H%M%S +0000',time.gmtime(time.time()+1200))
            body=('<tv>'+''.join(f'<programme channel="ch{i}" start="{start}" stop="{end}"><title>חדשות הערב</title><desc>Current programme fixture</desc></programme>' for i in range(500))+'</tv>').encode()
        elif counter=='playlist': body=playlist
        else:
            if action=='get_series_info':
                series=params.get('series_id',['1'])[0]
                data={'episodes':{'1':[{'id':int(series)*100+i,'title':f'Series S01E0{i} - Episode {i}','season':1,'episode_num':i,'container_extension':'mkv'} for i in (1,2)]+[{'id':int(series)*100+9,'title':'נראתה לאחרונה S01E01 - המוקד','season':1,'episode_num':1,'container_extension':'mkv'}],'2':[{'id':int(series)*100+3,'title':'Episode 3','season':2,'episode_num':1,'container_extension':'mkv'}]}}
            else:
                data={
                    'get_live_categories':[{'category_id':str(i),'category_name':f'Group {i}'} for i in range(10)],
                    'get_live_streams':[{'stream_id':i+1,'num':i+1,'name':f'Channel {i}','stream_icon':'/art/logo.svg','category_id':str(i%10),'tv_archive':1 if i==0 else i%2,'tv_archive_duration':7} for i in range(500)],
                    'get_vod_categories':[{'category_id':'1','category_name':'Movies'}],
                    'get_series_categories':[{'category_id':'1','category_name':'Series'}],
                    'get_vod_streams':[{'stream_id':i+1,'name':f'Movie {i:04d}'+(' · שלום' if i==0 else ''),'cover_big':['/art/poster.webp?size=large'],'category_id':'1','container_extension':'mp4'} for i in range(MOVIES)],
                    'get_series':[{'series_id':i+1,'name':f'Series {i:04d}','cover':'/art/poster.png','category_id':'1'} for i in range(SERIES)],
                    'get_short_epg':{'epg_listings':[{'title':base64.b64encode('חדשות הערב'.encode()).decode(),'description':'Test programme','start_timestamp':int(time.time())-600,'stop_timestamp':int(time.time())+1200}]},
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
            settings={'home_window_state':'0,1280x720,0x0','app_lang':locale,'iptv_mode':mode,'m3u8_url':url+'/playlist.m3u','xtream_server_url':url,'xtream_username':'fixture','xtream_password':'fixture','xtream_enabled':1}
            (config/'tsvitch_config.json').write_text(json.dumps({'setting':settings,'client':'fixture','device':'fixture'}))
            env=os.environ.copy();env['NO_PROXY']='localhost,127.0.0.1';env['no_proxy']=env['NO_PROXY']
            def run(flag):
                log=work/'ui.log'
                capture_root=os.environ.get('TSVITCH_UI_CAPTURE')
                finished=threading.Event()
                if capture_root and flag=='--ui-smoke-test':
                    capture=Path(capture_root)/locale;capture.mkdir(parents=True,exist_ok=True);env['TSVITCH_UI_CAPTURE']=str(capture)
                    def snapshots():
                        seen=set()
                        while not finished.wait(.03):
                            try: stage=int((capture/'stage').read_text())
                            except (OSError,ValueError):continue
                            if stage in (0,3,14,15,18,30,31) and stage not in seen:
                                seen.add(stage);subprocess.run(['scrot','-o',str(capture/f'{stage}.png')],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
                    thread=threading.Thread(target=snapshots,daemon=True);thread.start()
                else:env.pop('TSVITCH_UI_CAPTURE',None)
                try:
                    result=subprocess.run([str(binary),flag,'-d','-o',str(log)],cwd=work,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=65)
                    trace=log.read_text(errors='replace') if log.exists() else ''
                    assert result.returncode==0,(result.returncode,result.stdout[-2000:],trace[-5000:])
                    assert 'UI_SMOKE_PASSED' in trace,trace[-5000:]
                    assert f'locale: {locale}' in trace,trace[:5000]
                    assert 'received 500 channels' in trace,trace[-5000:]
                    assert 'Fatal error:' not in trace,trace[-5000:]
                except Exception:
                    print(log.read_text(errors='replace')[-6000:] if log.exists() else 'No startup log',file=sys.stderr)
                    raise
                finally:finished.set()
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
