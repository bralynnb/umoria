#!/usr/bin/env python3
"""Two-player Umoria HTTP host. Standard library only; one engine per room.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import json
import os
from pathlib import Path
import secrets
import hashlib
import sqlite3
import selectors
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / 'build-web'
MAX_ROOMS = int(os.environ.get('MAX_ROOMS', '8'))
ROOM_TTL = 30 * 60
SAVE_DIR = Path(os.environ.get('SAVE_DIR', str(ROOT / 'saves')))
ENGINE_VERSION = hashlib.sha256((BUILD / 'umoria-coop').read_bytes()).hexdigest()

rooms = {}
rooms_lock = threading.RLock()

class Journal:
    """Atomic, disk-backed log. Acknowledged actions are durable before replying."""
    def __init__(self, directory):
        directory.mkdir(parents=True, exist_ok=True, mode=0o700)
        self.lock = threading.RLock()
        self.db = sqlite3.connect(directory / 'rooms.sqlite3', check_same_thread=False)
        os.chmod(directory / 'rooms.sqlite3', 0o600)
        self.db.execute('PRAGMA journal_mode=WAL')
        self.db.execute('PRAGMA synchronous=FULL')
        self.db.executescript("""
            CREATE TABLE IF NOT EXISTS rooms (
              id TEXT PRIMARY KEY, seed INTEGER, clock INTEGER, version TEXT,
              tokens TEXT, state TEXT, seq INTEGER NOT NULL DEFAULT 0);
            CREATE TABLE IF NOT EXISTS commands (
              room TEXT, seq INTEGER, slot INTEGER, key INTEGER,
              PRIMARY KEY(room, seq));
        """)
    def create(self, room):
        with self.lock, self.db:
            self.db.execute('INSERT INTO rooms VALUES(?,?,?,?,?,?,0)',
                (room.id, room.seed, room.clock, ENGINE_VERSION, json.dumps(room.tokens), None))
    def read(self, room_id):
        with self.lock:
            row = self.db.execute('SELECT seed,clock,version,tokens,state,seq FROM rooms WHERE id=?', (room_id,)).fetchone()
            return row
    def events(self, room_id):
        with self.lock:
            return self.db.execute('SELECT slot,key FROM commands WHERE room=? ORDER BY seq', (room_id,)).fetchall()
    def append(self, room, slot, key):
        with self.lock, self.db:
            self.db.execute('INSERT INTO commands VALUES(?,?,?,?)', (room.id, room.seq+1, slot, key))
            self.db.execute('UPDATE rooms SET tokens=?,state=?,seq=? WHERE id=?',
                (json.dumps(room.tokens), json.dumps(room.state), room.seq+1, room.id))
        room.seq += 1

journal = Journal(SAVE_DIR)

class Room:
    def __init__(self, room_id=None, saved=None):
        self.id = room_id or secrets.token_urlsafe(18)
        self.lock = threading.RLock()
        self.tokens = [None, None]
        self.seen = [0., 0.]
        self.last_key = [0., 0.]
        self.last_active = time.monotonic()
        self.state = None
        self.process = None
        self.selector = None
        self.buffer = b''
        self.seq = 0
        self.seed = secrets.randbelow(2147483646) + 1
        self.clock = int(time.time())
        self.version = ENGINE_VERSION
        if saved:
            self.seed, self.clock, self.version, tokens, state, self.seq = saved
            self.tokens = json.loads(tokens)
            self.state = json.loads(state) if state else None
        else:
            journal.create(self)

    def close(self):
        if self.process is None: return
        self.process.terminate()
        try: self.process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        self.selector.close()
        self.process.stdin.close()
        self.process.stdout.close()
        self.process = None

    def ensure_running(self):
        if self.process is not None:
            if self.process.poll() is None: return
            self.close()
        if self.version != ENGINE_VERSION:
            raise ValueError('This saved room needs its original engine version. Your save has been preserved.')
        expected = self.state
        self.process = subprocess.Popen([str(BUILD / 'umoria-coop'), str(self.seed), str(self.clock)], cwd=BUILD,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, bufsize=0)
        self.buffer = b''
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.process.stdout, selectors.EVENT_READ)
        try:
            for slot, key in journal.events(self.id): self.exchange(slot, key)
            if expected is not None and self.state != expected:
                raise ValueError('Saved-room replay did not match. Your save has been preserved.')
        except Exception:
            self.close()
            self.state = expected
            raise

    def exchange(self, slot, key):
        self.process.stdin.write(f'{slot} {key}\n'.encode())
        deadline = time.monotonic() + 5
        while b'\n' not in self.buffer:
            if not self.selector.select(max(0, deadline - time.monotonic())):
                self.process.kill()
                raise ValueError('The dungeon stopped responding. Reload to restore its last saved action.')
            block = os.read(self.process.stdout.fileno(), 65536)
            if not block: raise ValueError('The dungeon stopped. Reload to restore its last saved action.')
            self.buffer += block
            if len(self.buffer) > 100000: raise ValueError('Invalid engine response.')
        line, self.buffer = self.buffer.split(b'\n', 1)
        self.state = json.loads(line)

    def command(self, slot, key):
        self.ensure_running()
        previous = self.state
        try:
            self.exchange(slot, key)
            journal.append(self, slot, key)
        except Exception:
            self.close()
            self.state = previous
            raise

    def join(self):
        with self.lock:
            for slot in range(2):
                if self.tokens[slot] is None:
                    token = secrets.token_urlsafe(32)
                    self.tokens[slot] = token
                    try: self.command(slot, -1)
                    except Exception:
                        self.tokens[slot] = None
                        raise
                    self.seen[slot] = self.last_active = time.monotonic()
                    return {'token': token, 'slot': slot}
            raise ValueError('This room already has two players. Use your original tab or personal resume link.')

    def authorize(self, token):
        if not token: raise PermissionError('Join the room first.')
        for i, expected in enumerate(self.tokens):
            if expected and secrets.compare_digest(expected, token): return i
        raise PermissionError('Invalid player session.')

    def snapshot(self, slot):
        self.ensure_running()
        self.seen[slot] = self.last_active = time.monotonic()
        result = {'slot': slot, 'depth': self.state['depth'], 'turn': self.state['turn'],
                  'saved': self.seq, 'self': self.state['players'][slot], 'players': []}
        for i, p in enumerate(self.state['players']):
            result['players'].append({k: p[k] for k in ('name', 'hp', 'joined', 'finished')})
            result['players'][-1]['connected'] = self.seen[i] > time.monotonic() - 5
        return result

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_): pass
    def reply(self, code, body, content_type='application/json'):
        if isinstance(body, (dict, list)): body = json.dumps(body).encode()
        if isinstance(body, str): body = body.encode()
        self.send_response(code)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Referrer-Policy', 'no-referrer')
        self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        try:
            path = urlsplit(self.path).path
            if path == '/health': return self.reply(200, {'ok': True})
            if path in ('/', '/app.js', '/style.css'):
                file = 'index.html' if path == '/' else path[1:]
                mime = {'index.html':'text/html; charset=utf-8','app.js':'text/javascript; charset=utf-8','style.css':'text/css; charset=utf-8'}[file]
                return self.reply(200, (ROOT/'web'/file).read_bytes(), mime)
            if path.startswith('/api/rooms/') and path.endswith('/state'):
                room = self.get_room(path.split('/')[3])
                with room.lock:
                    slot = room.authorize(self.headers.get('Authorization','').removeprefix('Bearer '))
                    return self.reply(200, room.snapshot(slot))
            self.reply(404, {'error':'Not found'})
        except PermissionError as e: self.reply(403, {'error':str(e)})
        except (ValueError, KeyError) as e: self.reply(400, {'error':str(e)})
        except (OSError, sqlite3.Error): self.reply(503, {'error':'Save storage is unavailable. Your last committed save is preserved.'})

    def get_room(self, room_id):
        with rooms_lock:
            if room_id not in rooms:
                saved = journal.read(room_id)
                if not saved: raise ValueError('Room not found. Check the invitation or resume link.')
                if len(rooms) >= MAX_ROOMS: raise ValueError('All active dungeon slots are busy. Try later; your save is safe.')
                rooms[room_id] = Room(room_id, saved)
            return rooms[room_id]

    def do_POST(self):
        try:
            origin = self.headers.get('Origin')
            if self.headers.get('Sec-Fetch-Site') == 'cross-site' or (origin and urlsplit(origin).netloc != self.headers.get('Host')):
                raise PermissionError('Cross-site requests are not allowed.')
            length = int(self.headers.get('Content-Length','0'))
            if length < 0 or length > 1024: raise ValueError('Request too large.')
            if self.headers.get_content_type() != 'application/json': raise ValueError('JSON required.')
            body = json.loads(self.rfile.read(length) or b'{}')
            if not isinstance(body, dict): raise ValueError('JSON object required.')
            path = urlsplit(self.path).path
            if path == '/api/rooms':
                with rooms_lock:
                    if len(rooms) >= MAX_ROOMS: raise ValueError('All dungeon rooms are busy. Try later.')
                    room = Room()
                    try: joined = room.join()
                    except Exception:
                        room.close()
                        raise
                    room_id = room.id
                    rooms[room_id] = room
                return self.reply(201, {'room':room_id, **joined})
            parts = path.split('/')
            if len(parts) != 5 or parts[1:3] != ['api','rooms']: return self.reply(404, {'error':'Not found'})
            room = self.get_room(parts[3])
            if parts[4] == 'join': return self.reply(200, room.join())
            if parts[4] == 'input':
                with room.lock:
                    slot = room.authorize(self.headers.get('Authorization','').removeprefix('Bearer '))
                    key = body.get('key')
                    if type(key) is not int or not 0 <= key <= 127: raise ValueError('Invalid key.')
                    now = time.monotonic()
                    if now - room.last_key[slot] < .035: return self.reply(429, {'error':'Please slow down.'})
                    room.last_key[slot] = now
                    room.command(slot, key)
                    return self.reply(200, room.snapshot(slot))
            self.reply(404, {'error':'Not found'})
        except PermissionError as e: self.reply(403, {'error':str(e)})
        except (ValueError, KeyError) as e: self.reply(400, {'error':str(e)})
        except (OSError, sqlite3.Error): self.reply(503, {'error':'Action was not saved. Your last committed save is preserved; reload before continuing.'})

def tick():
    while True:
        time.sleep(.25)
        with rooms_lock: current = list(rooms.items())
        for room_id, room in current:
            with room.lock:
                if time.monotonic() - room.last_active > ROOM_TTL:
                    with rooms_lock: rooms.pop(room_id, None)
                    room.close()
                    continue
                for slot in range(2):
                    if room.process and room.tokens[slot] and room.seen[slot] > time.monotonic()-5 and room.state['players'][slot]['automatic']:
                        try: room.command(slot, -2)
                        except (ValueError, BrokenPipeError, sqlite3.Error): break

if __name__ == '__main__':
    if not (BUILD/'umoria-coop').exists(): raise SystemExit('Run ./web/build.sh first.')
    threading.Thread(target=tick, daemon=True).start()
    address = (os.environ.get('HOST','127.0.0.1'), int(os.environ.get('PORT','8080')))
    server = ThreadingHTTPServer(address, Handler)
    server.daemon_threads = True
    print(f'Umoria co-op at http://{address[0]}:{address[1]}', flush=True)
    try: server.serve_forever()
    finally:
        for room in list(rooms.values()): room.close()
