import { Hono } from "hono";
import { transformToVector } from "./vectorized.js";
import { getTop5Neighbors } from "./ann.js";
import { analyzeFraud } from "./fraud.js";
import type { ResponseDTO } from "./dto/response.dto.js";

const routes = new Hono();

routes.get("/ready", (c) => c.text("Ready", 200));

routes.post("/fraud-score", async (c) => {
    const data = await c.req.raw.json() as ResponseDTO;
    const vector = transformToVector(data);
    const neighbors = await getTop5Neighbors(vector);
    return c.json(analyzeFraud(neighbors));
});

export default routes;