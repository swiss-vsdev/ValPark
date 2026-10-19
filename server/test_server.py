import unittest,os,tempfile,json,threading,urllib.request,urllib.error
from cryptography.fernet import Fernet
D="PK-A1B2C3D4E5F6"
os.environ['DB_PATH']=tempfile.mktemp(suffix='.db')
os.environ['DEVICE_TOKENS_JSON']=json.dumps({D:'device-secret'})
os.environ['PARKING_SECRET_KEY']=Fernet.generate_key().decode()
os.environ['ADMIN_TOKEN']='admin-secret'
import server
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  server.init_db();cls.http=server.ThreadingHTTPServer(('127.0.0.1',0),server.Handler)
  cls.url='http://127.0.0.1:'+str(cls.http.server_address[1]);threading.Thread(target=cls.http.serve_forever,daemon=True).start()
 @classmethod
 def tearDownClass(cls):cls.http.shutdown();cls.http.server_close()
 def post(self,path,data):
  req=urllib.request.Request(self.url+path,json.dumps(data).encode(),{'Content-Type':'application/json'})
  try:
   with urllib.request.urlopen(req) as r:return r.status,json.load(r)
  except urllib.error.HTTPError as e:return e.code,json.load(e)
 def test_cycle(self):
  auth={'device':D,'token':'device-secret'}
  e={**auth,'event_id':'test-event-001','state':'PARKED','lat':46.23,'lon':7.36,'disc':'11:30'}
  self.assertEqual(self.post('/api/event',e)[0],403)
  self.assertEqual(self.post('/api/register',{**auth,'pp_user':'fake-user','pp_pass':'fake-password'})[0],200)
  with open(server.DB,'rb') as f:self.assertNotIn(b'fake-password',f.read())
  self.assertEqual(self.post('/api/event',e)[0],200)
  self.assertTrue(self.post('/api/event',e)[1]['duplicate'])
  self.assertEqual(self.post('/api/event',{**e,'token':'bad'})[0],401)
  self.assertEqual(self.post('/api/event',{**e,'lat':float('nan')})[0],400)
  self.assertEqual(self.post('/api/event',{**e,'disc':'13:17'})[0],400)
  self.assertEqual(self.post('/api/event',{**e,'event_id':'test-event-002','state':'DEPART','disc':None})[0],200)
  with server.connect() as c:
   self.assertEqual(c.execute('SELECT COUNT(*) FROM events').fetchone()[0],2)
   self.assertIsNotNone(c.execute('SELECT ended FROM sessions').fetchone()[0])
 def test_register_validation(self):
  self.assertEqual(self.post('/api/register',[])[0],400)
  self.assertEqual(self.post('/api/register',{'device':D,'token':'bad'})[0],401)
  self.assertEqual(self.post('/api/register',{'device':D,'token':'device-secret','pp_user':'x','pp_pass':''})[0],400)
 def test_admin_not_public(self):
  try:urllib.request.urlopen(self.url+'/sessions')
  except urllib.error.HTTPError as e:self.assertEqual(e.code,401)
  else:self.fail('route non protegee')
if __name__=='__main__':unittest.main()
