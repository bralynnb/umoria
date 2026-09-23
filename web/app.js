'use strict';
const $ = id => document.getElementById(id);
let room = new URLSearchParams(location.hash.slice(1)).get('room');
let token = new URLSearchParams(location.hash.slice(1)).get('resume') || (room && sessionStorage.getItem('moria:' + room));
if(room && token) {
  sessionStorage.setItem('moria:'+room,token);
  history.replaceState(null,'', '/#'+new URLSearchParams({room}));
}
let playing = false, busy = false, queue = [], timer, latestSaved = -1;
const notice = text => { $('notice').textContent = text; };
async function request(path, body) {
  const response = await fetch(path, {method: body === undefined ? 'GET' : 'POST',
    headers: {'Content-Type':'application/json', ...(token ? {Authorization:'Bearer '+token} : {})},
    ...(body === undefined ? {} : {body:JSON.stringify(body)})});
  const data = await response.json();
  if (!response.ok) throw new Error(data.error || 'Connection failed.');
  return data;
}
function render(data) {
  if(data.saved < latestSaved) return;
  latestSaved = data.saved;
  const screen=data.self.screen; const rows=[];
  for(let i=0;i<24;i++) rows.push(screen.slice(i*80,(i+1)*80));
  $('terminal').textContent=rows.join('\n');
  $('room-status').textContent=`Player ${data.slot+1} · ${data.depth ? 'Depth '+(data.depth*50)+' feet' : 'Town'} · Turn ${data.turn} · Saved`;
  $('party').textContent=data.players.map((p,i)=>`P${i+1} ${p.name || 'Adventurer'} — ${p.finished?'finished':!p.connected?'away / waiting':!p.joined?'creating character':p.hp+' HP'}`).join('    /    ');
}
async function poll() {
  try {render(await request(`/api/rooms/${room}/state`));}
  catch(error) {notice(error.message);}
  timer=setTimeout(poll,300);
}
function enter() {
  playing=true; $('lobby').hidden=true; $('game').hidden=false;
  $('terminal').focus(); clearTimeout(timer); poll();
}
async function join(create) {
  $('create').disabled=$('join').disabled=true;
  try {
    const data=await request(create?'/api/rooms':`/api/rooms/${room}/join`,{});
    room=data.room || room;token=data.token;
    sessionStorage.setItem('moria:'+room,token);
    location.hash=new URLSearchParams({room}).toString();enter();
  } catch(error) {notice(error.message);}
  finally {$('create').disabled=$('join').disabled=false;}
}
async function drain() {
  if(busy || !queue.length) return;
  busy=true;
  try {render(await request(`/api/rooms/${room}/input`,{key:queue.shift()}));}
  catch(error) {notice(error.message);queue=[];}
  finally {setTimeout(()=>{busy=false;drain();},45);}
}
function send(key) {if(playing && queue.length<16) {queue.push(key);drain();}}
$('create').onclick=()=>join(true);
$('join').onclick=()=>join(false);
$('invite').onclick=async()=>{
  const url=location.origin+'/#'+new URLSearchParams({room});
  try {await navigator.clipboard.writeText(url);notice('Invitation copied. Send it to your companion.');}
  catch {notice('Invitation: '+url);}
  $('terminal').focus();
};
$('resume').onclick=async()=>{
  const url=location.origin+'/#'+new URLSearchParams({room,resume:token});
  try {await navigator.clipboard.writeText(url);notice('Personal resume link copied. Keep it private: it controls your character.');}
  catch {notice('Personal resume link (keep private): '+url);}
  $('terminal').focus();
};
for(const button of document.querySelectorAll('[data-key]')) button.onclick=()=>{send(Number(button.dataset.key));$('terminal').focus();};
$('terminal').addEventListener('keydown',event=>{
  if(event.metaKey || event.altKey) return;
  const special={ArrowUp:56,ArrowDown:50,ArrowLeft:52,ArrowRight:54,Home:55,PageUp:57,End:49,PageDown:51,Enter:13,Escape:27,Backspace:127,Delete:127};
  let key=special[event.key];
  if(event.ctrlKey && /^[a-z]$/i.test(event.key)) key=event.key.toUpperCase().charCodeAt(0)&31;
  else if(key===undefined && event.key.length===1 && event.key.charCodeAt(0)<128) key=event.key.charCodeAt(0);
  if(key!==undefined) {event.preventDefault();send(key);}
});
if(room) {$('join').hidden=false;$('create').textContent='Create a different dungeon';}
if(token) enter();
