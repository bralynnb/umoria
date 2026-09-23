import {database} from '@/lib/db';
import {apply,createWorld,tick,view,type Action,type World} from '@/lib/game';
export const dynamic='force-dynamic';
const response=(data:unknown,status=200,cookie?:string)=>Response.json(data,{status,headers:{'Cache-Control':'no-store',...(cookie?{'Set-Cookie':cookie}:{})}});
export async function POST(request:Request){
 const url=new URL(request.url),origin=request.headers.get('origin');if(origin&&origin!==url.origin)return response({error:'Origin not allowed'},403);
 if(Number(request.headers.get('content-length')||0)>2048)return response({error:'Request too large'},413);
 let a:Action;try{const body=await request.text();if(body.length>2048)return response({error:'Request too large'},413);a=JSON.parse(body);if(!a||typeof a.type!=='string')throw Error();}catch{return response({error:'Invalid action'},400);}
 const valid=['join','sync','move','attack','spell','potion','stairs','recall','buy','chat','leave'];if(!valid.includes(a.type))return response({error:'Unknown action'},400);
 let token=request.headers.get('cookie')?.match(/(?:^|;\s*)moria_session=([a-f0-9]{64})(?:;|$)/)?.[1];let cookie;
 if(!token){if(a.type!=='join')return response({joined:false,online:0});token=Array.from(crypto.getRandomValues(new Uint8Array(32)),n=>n.toString(16).padStart(2,'0')).join('');cookie=`moria_session=${token}; Path=/; HttpOnly; SameSite=Strict; Max-Age=31536000${url.protocol==='https:'?'; Secure':''}`;}
 const key=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(token))),n=>n.toString(16).padStart(2,'0')).join('');
 try{const db=database();const exists=await db.prepare('SELECT id FROM realms WHERE id=1').first();if(!exists)await db.prepare('INSERT OR IGNORE INTO realms (id,revision,state) VALUES (1,0,?)').bind(JSON.stringify(createWorld(Date.now()))).run();
 for(let attempt=0;attempt<10;attempt++){const row=await db.prepare('SELECT revision,state FROM realms WHERE id=1').first<{revision:number;state:string}>();if(!row)throw Error('Realm missing');const w=JSON.parse(row.state) as World,now=Date.now();tick(w,now);try{apply(w,key,a,now);}catch(e){return response({error:(e as Error).message},400);}const result=view(w,key,now);const update=await db.prepare('UPDATE realms SET state=?,revision=revision+1 WHERE id=1 AND revision=?').bind(JSON.stringify(w),row.revision).run();if(update.meta.changes)return response(result,200,cookie);}
 return response({error:'Realm is busy. Please try again.'},503);
 }catch(e){console.error('Realm request failed',e);return response({error:'The realm is temporarily unavailable. Please reconnect.'},503);}
}
