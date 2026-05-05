import { Hono } from "hono"
import { transformToVector } from "../services/vetorizedService"
import { getTop5Neighbors } from "../services/knn"
import { analyzeFraud } from "../services/fraud"
import type { ResponseDTO } from "../dto/response.dto"

const routes = new Hono()

routes.get("/ready", (c: any) => {
    return c.text("Ready", 200)
})

routes.post("/fraud-score", async (c) => {
    const data: ResponseDTO = await c.req.json();
    const vetorized = transformToVector(data);
    const neighbors = getTop5Neighbors(vetorized);
    const result = analyzeFraud(neighbors);

    console.log("instance:", process.env.HOSTNAME)

    if(!result.approved) {
        return c.json({
            "approved": false,
            "fraud_score": result.score
        })
    }

    return c.json({
        "approved": true,
        "fraud_score": result.score,
    })
    
})

export default routes;