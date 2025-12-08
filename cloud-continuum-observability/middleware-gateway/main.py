# middleware-gateway/main.py
from fastapi import FastAPI, HTTPException, BackgroundTasks
from pydantic import BaseModel
from datetime import datetime
from typing import Dict, Any, Optional
import redis
import logging
import json
import requests
from prometheus_client import Counter, Histogram, Gauge
import uvicorn
from prometheus_client import make_asgi_app

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = FastAPI(title="Observability Middleware Gateway", version="1.0.0")

# Monta l'app Prometheus per esporre /metrics
metrics_app = make_asgi_app()
app.mount("/metrics", metrics_app)

# Prometheus metrics
metrics_received = Counter(
    'middleware_metrics_received_total',
    'Total metrics received',
    ['agent_type', 'location']
)
processing_time = Histogram(
    'middleware_processing_seconds',
    'Time spent processing metrics'
)
active_agents = Gauge(
    'middleware_active_agents',
    'Number of active agents',
    ['agent_type']
)

# Redis connection
redis_client = redis.Redis(
    host='redis',
    port=6379,
    decode_responses=True
)

class MetricPayload(BaseModel):
    agent_id: str
    location: str
    agent_type: str
    timestamp: str
    metrics: Dict[str, Any]

class HealthResponse(BaseModel):
    status: str
    timestamp: str
    components: Dict[str, str]

@app.get("/health", response_model=HealthResponse)
async def health_check():
    """Health check endpoint"""
    components = {}
    
    # Check Redis
    try:
        redis_client.ping()
        components['redis'] = 'healthy'
    except:
        components['redis'] = 'unhealthy'
    
    # Check Processor
    try:
        resp = requests.get('http://middleware-processor:8001/health', timeout=2)
        components['processor'] = 'healthy' if resp.status_code == 200 else 'unhealthy'
    except:
        components['processor'] = 'unhealthy'
    
    overall_status = 'healthy' if all(v == 'healthy' for v in components.values()) else 'degraded'
    
    return HealthResponse(
        status=overall_status,
        timestamp=datetime.utcnow().isoformat(),
        components=components
    )

@app.post("/api/v1/metrics")
async def receive_metrics(payload: MetricPayload, background_tasks: BackgroundTasks):
    """
    Riceve metriche dagli edge agents
    - Valida i dati
    - Bufferizza in Redis
    - Invia al processor per elaborazione
    """
    try:
        with processing_time.time():
            # Incrementa counter
            metrics_received.labels(
                agent_type=payload.agent_type,
                location=payload.location
            ).inc()
            
            # Bufferizza in Redis (con TTL di 1 ora)
            cache_key = f"metrics:{payload.agent_id}:{payload.timestamp}"
            redis_client.setex(
                cache_key,
                3600,  # TTL: 1 ora
                json.dumps(payload.dict())
            )
            
            # Mantieni lista degli ultimi 100 messaggi per agent
            list_key = f"agent_history:{payload.agent_id}"
            redis_client.lpush(list_key, cache_key)
            redis_client.ltrim(list_key, 0, 99)
            
            # Aggiorna set di agenti attivi
            redis_client.sadd(f"active_agents:{payload.agent_type}", payload.agent_id)
            redis_client.expire(f"active_agents:{payload.agent_type}", 300)
            
            # Aggiorna gauge Prometheus
            for agent_type in ['temperature-sensor', 'humidity-sensor', 'gateway']:
                count = redis_client.scard(f"active_agents:{agent_type}")
                active_agents.labels(agent_type=agent_type).set(count)
            
            # Invia al processor in background
            background_tasks.add_task(forward_to_processor, payload)
            
            logger.info(f"Received metrics from {payload.agent_id} ({payload.agent_type})")
            
            return {
                "status": "accepted",
                "agent_id": payload.agent_id,
                "timestamp": datetime.utcnow().isoformat()
            }
            
    except Exception as e:
        logger.error(f"Error processing metrics: {e}")
        raise HTTPException(status_code=500, detail=str(e))

async def forward_to_processor(payload: MetricPayload):
    """Inoltra metriche al processor per elaborazione"""
    try:
        response = requests.post(
            'http://middleware-processor:8001/api/v1/process',
            json=payload.dict(),
            timeout=5
        )
        if response.status_code != 200:
            logger.warning(f"Processor returned {response.status_code}")
    except Exception as e:
        logger.error(f"Failed to forward to processor: {e}")

@app.get("/api/v1/agents")
async def list_agents():
    """Lista tutti gli agenti attivi"""
    agents = {}
    for agent_type in ['temperature-sensor', 'humidity-sensor', 'gateway']:
        agent_ids = redis_client.smembers(f"active_agents:{agent_type}")
        agents[agent_type] = list(agent_ids)
    
    return {
        "active_agents": agents,
        "total_count": sum(len(v) for v in agents.values()),
        "timestamp": datetime.utcnow().isoformat()
    }

@app.get("/api/v1/agent/{agent_id}/history")
async def get_agent_history(agent_id: str, limit: int = 10):
    """Recupera storico metriche di un agente da Redis"""
    list_key = f"agent_history:{agent_id}"
    history_keys = redis_client.lrange(list_key, 0, limit - 1)
    
    history = []
    for key in history_keys:
        data = redis_client.get(key)
        if data:
            history.append(json.loads(data))
    
    return {
        "agent_id": agent_id,
        "history": history,
        "count": len(history)
    }

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)