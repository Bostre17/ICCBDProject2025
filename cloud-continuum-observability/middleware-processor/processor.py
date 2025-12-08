# middleware-processor/processor.py
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from datetime import datetime
from typing import Dict, Any
import logging
from prometheus_client import CollectorRegistry, Gauge, push_to_gateway, make_asgi_app
import uvicorn

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = FastAPI(title="Observability Middleware Processor", version="1.0.0")

# Monta l'app Prometheus per esporre /metrics
metrics_app = make_asgi_app()
app.mount("/metrics", metrics_app)

class MetricPayload(BaseModel):
    agent_id: str
    location: str
    agent_type: str
    timestamp: str
    metrics: Dict[str, Any]

class ProcessingPipeline:
    """Pipeline per elaborazione, normalizzazione e arricchimento dati"""
    
    def __init__(self):
        self.pushgateway_url = 'pushgateway:9091'
        
    def normalize(self, payload: MetricPayload) -> Dict[str, Any]:
        """Normalizza i dati in formato standard"""
        normalized = {
            'source': {
                'agent_id': payload.agent_id,
                'location': payload.location,
                'type': payload.agent_type
            },
            'timestamp': payload.timestamp,
            'metrics': {}
        }
        
        # Normalizza nomi metriche
        for key, value in payload.metrics.items():
            metric_name = f"{payload.agent_type}_{key}".replace('-', '_')
            normalized['metrics'][metric_name] = value
        
        return normalized
    
    def enrich(self, normalized_data: Dict[str, Any]) -> Dict[str, Any]:
        """Arricchisce i dati con metadata aggiuntivi"""
        enriched = normalized_data.copy()
        
        # Aggiungi metadata di processing
        enriched['processing'] = {
            'processed_at': datetime.utcnow().isoformat(),
            'pipeline_version': '1.0.0'
        }
        
        # Aggiungi classificazione layer
        if normalized_data['source']['type'] in ['temperature-sensor', 'humidity-sensor']:
            enriched['source']['layer'] = 'edge'
        elif normalized_data['source']['type'] == 'gateway':
            enriched['source']['layer'] = 'fog'
        else:
            enriched['source']['layer'] = 'unknown'
        
        # Calcola metriche derivate
        if 'temperature' in str(normalized_data['metrics']):
            # Converti eventualmente da Celsius a Fahrenheit
            for key, value in normalized_data['metrics'].items():
                if 'temperature' in key and isinstance(value, (int, float)):
                    enriched['metrics'][key + '_fahrenheit'] = round(value * 9/5 + 32, 2)
        
        return enriched
    
    def validate(self, payload: MetricPayload) -> bool:
        """Valida i dati ricevuti"""
        # Check campi obbligatori
        if not payload.agent_id or not payload.agent_type:
            return False
        
        # Check formato timestamp
        try:
            datetime.fromisoformat(payload.timestamp.replace('Z', '+00:00'))
        except:
            return False
        
        # Check metriche non vuote
        if not payload.metrics:
            return False
        
        return True
    
    def push_to_prometheus(self, enriched_data: Dict[str, Any]):
        """Invia metriche a Prometheus via Pushgateway"""
        try:
            registry = CollectorRegistry()
            
            agent_id = enriched_data['source']['agent_id']
            location = enriched_data['source']['location']
            agent_type = enriched_data['source']['type']
            
            for metric_name, metric_value in enriched_data['metrics'].items():
                if isinstance(metric_value, (int, float)):
                    # Crea gauge per ogni metrica
                    gauge = Gauge(
                        metric_name,
                        f'Metric from {agent_type}',
                        ['agent_id', 'location', 'layer'],
                        registry=registry
                    )
                    gauge.labels(
                        agent_id=agent_id,
                        location=location,
                        layer=enriched_data['source']['layer']
                    ).set(metric_value)
            
            # Push a Prometheus
            push_to_gateway(
                self.pushgateway_url,
                job=f'edge_agent_{agent_type}',
                registry=registry
            )
            
            logger.info(f"Pushed metrics to Prometheus for {agent_id}")
            
        except Exception as e:
            logger.error(f"Failed to push to Prometheus: {e}")
            raise

pipeline = ProcessingPipeline()

@app.get("/health")
async def health_check():
    """Health check endpoint"""
    return {
        "status": "healthy",
        "timestamp": datetime.utcnow().isoformat()
    }

@app.post("/api/v1/process")
async def process_metrics(payload: MetricPayload):
    """
    Processing pipeline completo:
    1. Valida
    2. Normalizza
    3. Arricchisce
    4. Invia a Prometheus
    """
    try:
        # Step 1: Validazione
        if not pipeline.validate(payload):
            raise HTTPException(status_code=400, detail="Invalid payload")
        
        # Step 2: Normalizzazione
        normalized = pipeline.normalize(payload)
        logger.info(f"Normalized data from {payload.agent_id}")
        
        # Step 3: Arricchimento
        enriched = pipeline.enrich(normalized)
        logger.info(f"Enriched data from {payload.agent_id}")
        
        # Step 4: Push a Prometheus
        pipeline.push_to_prometheus(enriched)
        
        return {
            "status": "processed",
            "agent_id": payload.agent_id,
            "processed_at": datetime.utcnow().isoformat(),
            "metrics_count": len(enriched['metrics'])
        }
        
    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Processing error: {e}")
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/api/v1/stats")
async def get_stats():
    """Statistiche del processor"""
    return {
        "pipeline_version": "1.0.0",
        "status": "running",
        "timestamp": datetime.utcnow().isoformat()
    }

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8001)