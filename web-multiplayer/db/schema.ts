import {integer,sqliteTable,text} from 'drizzle-orm/sqlite-core';
export const realms=sqliteTable('realms',{id:integer('id').primaryKey(),revision:integer('revision').notNull().default(0),state:text('state').notNull()});
