# edge-agent/agent.py
import os
import time
import random
import logging
from datetime import datetime
from prometheus_client import CollectorRegistry, Gauge, push_to_gateway
import requests

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class EdgeAgent:
    def __init__(self):
        self.agent_id = os.getenv('AGENT_ID', 'edge-unknown')
        self.location = os.getenv('AGENT_LOCATION', 'unknown')
        self.agent_type = os.getenv('AGENT_TYPE', 'generic')
        self.middleware_url = os.getenv('MIDDLEWARE_URL', 'http://middleware-gateway:8000')
        self.push_interval = int(os.getenv('PUSH_INTERVAL', '10'))
        
        # Registry per Prometheus
        self.registry = CollectorRegistry()
        
        # Metriche specifiche per tipo di sensore
        self.setup_metrics()
        
    def setup_metrics(self):
        """Setup metriche basate sul tipo di agente"""
        if self.agent_type == 'temperature-sensor':
            self.metric = Gauge(
                'edge_temperature_celsius',
                'Temperature in Celsius',
                ['agent_id', 'location'],
                registry=self.registry
            )
        elif self.agent_type == 'humidity-sensor':
            self.metric = Gauge(
                'edge_humidity_percent',
                'Humidity percentage',
                ['agent_id', 'location'],
                registry=self.registry
            )
        elif self.agent_type == 'gateway':
            self.cpu_metric = Gauge(
                'edge_gateway_cpu_usage',
                'Gateway CPU usage',
                ['agent_id', 'location'],
                registry=self.registry
            )
            self.memory_metric = Gauge(
                'edge_gateway_memory_usage',
                'Gateway memory usage MB',
                ['agent_id', 'location'],
                registry=self.registry
            )
            self.connections_metric = Gauge(
                'edge_gateway_active_connections',
                'Active connections',
                ['agent_id', 'location'],
                registry=self.registry
            )
    
    def generate_data(self):
        """Genera dati simulati basati sul tipo di sensore"""
        if self.agent_type == 'temperature-sensor':
            # Temperatura tra 18-28°C con variazioni graduali
            temp = 23 + random.uniform(-5, 5) + random.gauss(0, 1)
            self.metric.labels(
                agent_id=self.agent_id,
                location=self.location
            ).set(temp)
            return {'temperature': round(temp, 2)}
            
        elif self.agent_type == 'humidity-sensor':
            # Umidità tra 40-70%
            humidity = 55 + random.uniform(-15, 15) + random.gauss(0, 3)
            humidity = max(0, min(100, humidity))
            self.metric.labels(
                agent_id=self.agent_id,
                location=self.location
            ).set(humidity)
            return {'humidity': round(humidity, 2)}
            
        elif self.agent_type == 'gateway':
            # Gateway: CPU, memoria, connessioni
            cpu = random.uniform(10, 80)
            memory = random.uniform(100, 800)
            connections = random.randint(5, 50)
            
            self.cpu_metric.labels(
                agent_id=self.agent_id,
                location=self.location
            ).set(cpu)
            self.memory_metric.labels(
                agent_id=self.agent_id,
                location=self.location
            ).set(memory)
            self.connections_metric.labels(
                agent_id=self.agent_id,
                location=self.location
            ).set(connections)
            
            return {
                'cpu_usage': round(cpu, 2),
                'memory_mb': round(memory, 2),
                'connections': connections
            }
    
    def push_to_middleware(self):
        """Invia metriche al middleware via API"""
        try:
            data = self.generate_data()
            payload = {
                'agent_id': self.agent_id,
                'location': self.location,
                'agent_type': self.agent_type,
                'timestamp': datetime.utcnow().isoformat(),
                'metrics': data
            }
            
            response = requests.post(
                f"{self.middleware_url}/api/v1/metrics",
                json=payload,
                timeout=5
            )
            
            if response.status_code == 200:
                logger.info(f"[{self.agent_id}] Metrics sent successfully: {data}")
            else:
                logger.error(f"[{self.agent_id}] Failed to send metrics: {response.status_code}")
                
        except Exception as e:
            logger.error(f"[{self.agent_id}] Error sending to middleware: {e}")
    
    def run(self):
        """Main loop dell'agente"""
        logger.info(f"Starting Edge Agent: {self.agent_id} ({self.agent_type}) at {self.location}")
        
        # Attendi che il middleware sia pronto
        time.sleep(10)
        
        while True:
            try:
                self.push_to_middleware()
                time.sleep(self.push_interval)
            except KeyboardInterrupt:
                logger.info(f"[{self.agent_id}] Shutting down...")
                break
            except Exception as e:
                logger.error(f"[{self.agent_id}] Unexpected error: {e}")
                time.sleep(5)

if __name__ == "__main__":
    agent = EdgeAgent()
    agent.run()