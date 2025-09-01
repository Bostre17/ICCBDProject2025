// k6-script.js
import http from 'k6/http';
import { sleep } from 'k6';

export const options = {
  // Simula 50 utenti concorrenti
  vus: 50,
  // Durata del test: 10 minuti
  duration: '10m',
};

export default function () {
  // Accede all'applicazione tramite il load balancer NGINX
  // k6, essendo in Docker, userà il nome del servizio 'nginx'
  http.get('http://nginx');
  sleep(1); // Attende 1 secondo tra le richieste
}