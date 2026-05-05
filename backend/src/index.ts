import { serve } from '@hono/node-server'
import { Hono } from 'hono'
import routes from './controller/api'

const server= new Hono()

server.route("/", routes)

serve({
  fetch: server.fetch,
  port: 9999
})

