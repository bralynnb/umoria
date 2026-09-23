import test from 'node:test';
import {readFile} from 'node:fs/promises';
import {createRequire} from 'node:module';
import ts from 'typescript';
const require=createRequire(import.meta.resolve('wrangler/package.json'));
const {Miniflare}=require('miniflare');
import {runSmoke} from './api-smoke.mjs';
test('two independent players use the real route and real local D1 atomically',async()=>{
 const modules=[{type:'ESModule',path:'worker.js',contents:"import {POST} from './route.js'; export default {fetch:POST};"}];
 for(const [file,path] of [['lib/game.ts','game.js'],['lib/db.ts','db.js'],['app/api/game/route.ts','route.js']]){
 const source=(await readFile(new URL('../'+file,import.meta.url),'utf8')).replaceAll('@/lib/db','./db.js').replaceAll('@/lib/game','./game.js');
 modules.push({type:'ESModule',path,contents:ts.transpileModule(source,{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ESNext}}).outputText});
 }
 const mf=new Miniflare({modules,modulesRoot:process.cwd(),compatibilityDate:'2026-01-01',d1Databases:['DB']});
 try{const db=await mf.getD1Database('DB');await db.prepare('CREATE TABLE realms (id INTEGER PRIMARY KEY,revision INTEGER NOT NULL DEFAULT 0,state TEXT NOT NULL)').run();await runSmoke((url,options)=>mf.dispatchFetch(url,options),'https://game.test/api/game');}finally{await mf.dispose();}
});
