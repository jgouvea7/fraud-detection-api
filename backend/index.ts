import { Hono } from "hono";
import routes from "./services/api/controller.js";
import { serve } from "@hono/node-server";

const app = new Hono();
app.route("/", routes);

serve({
    fetch: app.fetch, port: 9999
});