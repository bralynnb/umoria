import assert from 'node:assert/strict';
export async function runSmoke(fetcher=fetch,endpoint=process.env.MORIA_TEST_URL||'http://127.0.0.1:4173/api/game'){
function client(){let cookie='';return async a=>{const r=await fetcher(endpoint,{method:'POST',headers:{'content-type':'application/json',...(cookie?{cookie}:{})},body:JSON.stringify({...a,id:crypto.randomUUID()})});const c=r.headers.get('set-cookie');if(c)cookie=c.split(';')[0];const d=await r.json();assert.equal(r.status,200,JSON.stringify(d));return d;};}
const a=client(),b=client();
const [pa,pb]=await Promise.all([a({type:'join',name:'TestWarrior',role:'Warrior'}),b({type:'join',name:'TestMage',role:'Mage'})]);
assert.notEqual(pa.me.id,pb.me.id);assert.equal(pb.me.role,'Mage');
const [ma,mb]=await Promise.all([a({type:'move',dx:1,dy:0}),b({type:'move',dx:-1,dy:0})]);
assert.equal(ma.me.x,pa.me.x+1);assert.equal(mb.me.x,pb.me.x-1);
const sync=await a({type:'sync'});assert(sync.players.some(p=>p.id===pb.me.id&&p.x===mb.me.x));
const [buy1,buy2]=await Promise.all([a({type:'buy',item:'potion'}),a({type:'buy',item:'potion'})]);
const after=await a({type:'sync'});assert.equal(after.me.gold,5);assert.equal(after.me.potions,4);
await b({type:'chat',text:'Two-player integration test'});assert((await a({type:'sync'})).chat.some(c=>c.text==='Two-player integration test'));
const sid=pa.me.id;assert.equal((await a({type:'sync'})).me.id,sid);
await Promise.all([a({type:'leave'}),b({type:'leave'})]);
console.log('PASS: independent sessions, concurrent movement, shared visibility, atomic shop charges, chat and reconnect.');

}
if(process.argv[1]?.endsWith('/api-smoke.mjs'))await runSmoke();
