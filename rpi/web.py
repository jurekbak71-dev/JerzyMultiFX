#!/usr/bin/env python3
import argparse, hashlib, hmac, http.server, json, os, secrets, urllib.parse
from pathlib import Path
from control import rpc
import hardware
import tempfile
from http.cookies import SimpleCookie

class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    disable_nagle_algorithm = True
    def log_message(self, *args):
        pass  # Do not log authentication values or uploaded data.
    def send(self, code, data, mime='application/json', session=False):
        if not isinstance(data, bytes): data=json.dumps(data,ensure_ascii=False).encode()
        self.send_response(code)
        self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'")
        if session:
            self.send_header('Set-Cookie', 'multifx-session='+self.server.token+'; Path=/; HttpOnly; SameSite=Strict')
        self.end_headers();self.wfile.write(data)
    def authorized(self):
        token=self.headers.get('Authorization','').removeprefix('Bearer ')
        cookie=SimpleCookie()
        try: cookie.load(self.headers.get('Cookie',''))
        except Exception: return False
        session=cookie.get('multifx-session')
        valid=hmac.compare_digest(token.encode(),self.server.token.encode()) or (session is not None and hmac.compare_digest(session.value.encode(),self.server.token.encode()))
        origin=self.headers.get('Origin')
        return valid and self.headers.get('Sec-Fetch-Site') != 'cross-site' and (origin is None or origin in ('http://'+self.headers.get('Host',''),'https://'+self.headers.get('Host','')))
    def do_GET(self):
        try:
            if self.path in ('/','/index.html'):
                return self.send(200,(self.server.ui/'index.html').read_bytes(),'text/html; charset=utf-8',session=True)
            if self.path in ('/metadata.js','/app.js','/style.css'):
                filename=self.path.removeprefix('/')
                return self.send(200,(self.server.ui/filename).read_bytes(),'text/css; charset=utf-8' if filename.endswith('.css') else 'text/javascript; charset=utf-8')
            if self.path == '/metadata.js':
                return self.send(200,(self.server.ui/'metadata.js').read_bytes(),'text/javascript; charset=utf-8')
            if not self.authorized(): return self.send(401,{'error':'Token required'})
            if self.path == '/api/source':
                source=self.server.ui.parent/'source.tar.gz'
                if not source.exists(): return self.send(404,{'error':'Source archive is included in binary packages'})
                return self.send(200,source.read_bytes(),'application/gzip')
            if self.path == '/api/presets':
                directory=self.server.data/'presets'
                names=sorted(p.stem for p in directory.glob('*.jmfx') if p.is_file() and not p.is_symlink() and 1<=len(p.stem)<=64 and all(c.isalnum() or c in ' -_' for c in p.stem))
                return self.send(200,{'ok':True,'presets':names})
            if self.path == '/api/hardware': return self.send(200,hardware.status(self.server.data))
            if self.path == '/api/state': return self.send(200,rpc('STATE'))
            if self.path == '/api/metadata': return self.send(200,rpc('METADATA'))
            return self.send(404,{'error':'Unknown resource'})
        except (OSError,ValueError,KeyError): self.send(503,{'error':'Audio core unavailable'})
    def do_POST(self):
        if not self.authorized():
            self.close_connection=True
            return self.send(403,{'error':'Unauthorized or invalid origin'})
        try:
            length=int(self.headers.get('Content-Length','0'))
            if not 0 < length <= 1024*1024:
                self.close_connection=True
                return self.send(413,{'error':'Size limit'})
            body=self.rfile.read(length)
            if len(body)!=length: raise ValueError('Truncated body')
            if self.path == '/api/preset':
                # Let the common C++ decoder verify CRC, IDs, embedded IR and version.
                directory=self.server.data/'presets';directory.mkdir(parents=True,exist_ok=True)
                with tempfile.NamedTemporaryFile(dir=directory,suffix='.jmfx',delete=False) as f:
                    f.write(body);path=Path(f.name)
                try:
                    result=rpc('LOAD '+str(path))
                    return self.send(200 if result.get('ok') else 422,result)
                finally:path.unlink(missing_ok=True)
            if self.path == '/api/ir':
                # A content-addressed controlled library; no client filesystem paths.
                name=hashlib.sha256(body).hexdigest()+'.wav'
                path=self.server.data/'ir'/name
                path.parent.mkdir(parents=True,exist_ok=True)
                if not path.exists(): path.write_bytes(body)
                result=rpc('IR '+str(path))
                if not result.get('ok'): path.unlink(missing_ok=True)
                else:
                    name=Path(urllib.parse.unquote(self.headers.get('X-IR-Name',''))).name
                    if name and len(name)<=128 and all(ord(c)>=32 for c in name):rpc('IRNAME '+name)
                return self.send(200 if result.get('ok') else 422,result)
            p=json.loads(body)
            if self.path == '/api/hardware': return self.send(200,hardware.action(p))
            if self.path == '/api/view':
                fields=[p.get(k,0) for k in ('block','section','mxSlot','advanced')]
                if any(type(v) is not int for v in fields):raise ValueError('View')
                return self.send(200,rpc('VIEW '+' '.join(map(str,fields))))
            if self.path == '/api/set':
                i,v=p['id'],p['value']
                if type(i) is not int or not isinstance(v,(float,int)) or not 0<=v<=1: raise ValueError('Parameter')
                return self.send(200,rpc(f'SET {i} {v}'))
            if self.path in ('/api/save','/api/load'):
                name=p['name']
                if not isinstance(name,str) or not name or len(name)>64 or any(not(c.isalnum() or c in ' -_') for c in name) or name!=name.strip(): raise ValueError('Preset name')
                path=self.server.data/'presets'/(name+'.jmfx');path.parent.mkdir(parents=True,exist_ok=True)
                return self.send(200,rpc(('SAVE ' if self.path=='/api/save' else 'LOAD ')+str(path)))
            return self.send(404,{'error':'Unknown operation'})
        except (ValueError,KeyError,TypeError,OSError) as e: return self.send(400,{'error':str(e) or 'Invalid request or unavailable core'})

def main():
    p=argparse.ArgumentParser();p.add_argument('--host',default='127.0.0.1');p.add_argument('--port',type=int,default=8097);p.add_argument('--data',type=Path,required=True);p.add_argument('--ui',type=Path,default=Path(__file__).resolve().parents[1]/'ui');a=p.parse_args()
    a.data.mkdir(parents=True,exist_ok=True);tokenfile=a.data/'token'
    if not tokenfile.exists():
        fd=os.open(tokenfile,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
        with os.fdopen(fd,'w') as f:f.write(secrets.token_urlsafe(32))
    server=http.server.ThreadingHTTPServer((a.host,a.port),Handler);server.token=tokenfile.read_text().strip();server.ui=a.ui;server.data=a.data
    if len(server.token)<32: raise SystemExit('Invalid token file')
    server.serve_forever()
if __name__=='__main__':main()
