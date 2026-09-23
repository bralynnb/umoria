# SPDX-License-Identifier: GPL-3.0-or-later
import concurrent.futures
import json
import os
from pathlib import Path
import socket
import subprocess
import time
import unittest
import urllib.request
import urllib.error

ROOT=Path(__file__).resolve().parent.parent
class BrowserServerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with socket.socket() as sock:
            sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
        cls.url=f'http://127.0.0.1:{port}'
        cls.server=subprocess.Popen(['python3','web/server.py'],cwd=ROOT,
            env={**os.environ,'PORT':str(port)},stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        assert 'Umoria co-op' in cls.server.stdout.readline()
    @classmethod
    def tearDownClass(cls):
        cls.server.terminate();cls.server.wait(timeout=5)
        cls.server.stdout.close();cls.server.stderr.close()
    def api(self,path,body=None,token=None,origin=None):
        headers={'Content-Type':'application/json'}
        if token:headers['Authorization']='Bearer '+token
        if origin:headers['Origin']=origin
        req=urllib.request.Request(self.url+path,data=None if body is None else json.dumps(body).encode(),headers=headers)
        try:
            with urllib.request.urlopen(req,timeout=10) as response:return response.status,json.load(response)
        except urllib.error.HTTPError as error:return error.code,json.load(error)
    def test_two_browser_clients(self):
        code,a=self.api('/api/rooms',{});self.assertEqual(code,201)
        room=a['room'];base='/api/rooms/'+room
        code,b=self.api(base+'/join',{});self.assertEqual(code,200)
        self.assertNotEqual(a['token'],b['token']);self.assertEqual((a['slot'],b['slot']),(0,1))
        self.assertEqual(self.api(base+'/join',{})[0],400)
        self.assertEqual(self.api(base+'/state',token='incorrect')[0],403)
        self.assertEqual(self.api(base+'/input',{'key':999},a['token'])[0],400)
        self.assertEqual(self.api('/api/rooms',{},origin='https://evil.example')[0],403)
        def create_player(player,name):
            for c in ' am\x1ba'+name+'\n ':
                time.sleep(.06)
                code,result=self.api(base+'/input',{'key':ord(c)},player['token'])
                self.assertEqual(code,200,result)
            return result
        with concurrent.futures.ThreadPoolExecutor(2) as pool:
            f1=pool.submit(create_player,a,'Alice');f2=pool.submit(create_player,b,'Bob')
            f1.result();f2.result()
        state_a=self.api(base+'/state',token=a['token'])[1]
        state_b=self.api(base+'/state',token=b['token'])[1]
        self.assertEqual(state_a['self']['name'],'Alice');self.assertEqual(state_b['self']['name'],'Bob')
        self.assertTrue(state_a['self']['joined'] and state_b['self']['joined'])
        self.assertNotEqual((state_a['self']['x'],state_a['self']['y']),(state_b['self']['x'],state_b['self']['y']))
        self.assertIn('&',state_a['self']['screen']);self.assertIn('&',state_b['self']['screen'])
        self.assertEqual(len(state_a['self']['screen']),1920)
        self.assertNotIn('screen',state_a['players'][1])
        # Both can submit actions concurrently. Reconnecting retains character.
        time.sleep(.06)
        with concurrent.futures.ThreadPoolExecutor(2) as pool:
            results=list(pool.map(lambda p:self.api(base+'/input',{'key':ord('5')},p['token']),[a,b]))
        self.assertTrue(all(code==200 for code,_ in results))
        self.assertEqual(self.api(base+'/state',token=a['token'])[1]['self']['name'],'Alice')
    def test_health_and_assets(self):
        self.assertEqual(self.api('/health')[1],{'ok':True})
        for path in ['/','/app.js','/style.css']:
            with urllib.request.urlopen(self.url+path) as response:
                self.assertEqual(response.status,200)
                self.assertIn("frame-ancestors 'none'",response.headers['Content-Security-Policy'])
                self.assertGreater(len(response.read()),100)
if __name__=='__main__': unittest.main()
