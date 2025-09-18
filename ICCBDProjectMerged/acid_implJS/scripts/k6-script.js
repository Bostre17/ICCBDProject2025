import http from 'k6/http';
import { sleep } from 'k6';

export const options = {
  vus: 50,
  duration: '3m',
};

// Lo script ora legge l'URL del target da una variabile d'ambiente
const target = __ENV.K6_HTTP_REQ_URL || 'http://nginx-base';

export default function () {
  http.get(target);
  sleep(1);
}