import {env} from 'cloudflare:workers';
export function database(){const db=(env as unknown as {DB:D1Database}).DB;if(!db)throw Error('Realm database unavailable');return db;}
