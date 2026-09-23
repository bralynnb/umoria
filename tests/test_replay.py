# SPDX-License-Identifier: GPL-3.0-or-later
"""Recovery and failure-mode checks against the real engine, without HTTP."""
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest

class RecoveryTest(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        os.environ['SAVE_DIR']=self.temp.name
        spec=importlib.util.spec_from_file_location('moria_server',Path(__file__).resolve().parents[1]/'web/server.py')
        self.server=importlib.util.module_from_spec(spec);spec.loader.exec_module(self.server)
        self.room=self.server.Room();self.room.join();self.room.join()
        for slot,name in [(0,'Alice'),(1,'Bob')]:
            for c in ' am\x1ba'+name+'\n ':self.room.command(slot,ord(c))
    def tearDown(self):
        self.room.close();self.server.journal.db.close();self.temp.cleanup()
    def test_replay_after_automatic_turns(self):
        # Rest advances via bounded scheduler ticks. They must replay, too.
        for c in 'R10\r':self.room.command(0,ord(c))
        self.assertTrue(self.room.state['players'][0]['automatic'])
        turn=self.room.state['turn']
        for _ in range(4):self.room.command(0,-2)
        self.assertGreater(self.room.state['turn'],turn)
        expected=self.room.state
        self.room.close();self.room.ensure_running()
        self.assertEqual(self.room.state,expected)
    def test_failed_save_does_not_acknowledge_unsaved_state(self):
        previous=self.room.state;seq=self.room.seq
        original=self.server.journal.append
        def fail(*args):raise OSError('disk full')
        self.server.journal.append=fail
        with self.assertRaises(OSError):self.room.command(0,ord('6'))
        self.server.journal.append=original
        self.assertIsNone(self.room.process);self.assertEqual(self.room.seq,seq)
        self.room.ensure_running();self.assertEqual(self.room.state,previous)
    def test_wrong_engine_preserves_save(self):
        self.room.close();self.room.version='other-version'
        with self.assertRaisesRegex(ValueError,'original engine version'):self.room.ensure_running()
        self.assertIsNotNone(self.server.journal.read(self.room.id))
        self.assertIsNone(self.room.process)
if __name__=='__main__':unittest.main()
