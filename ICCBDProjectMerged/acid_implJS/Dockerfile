# Dockerfile
FROM node:18-alpine

# Installa 'curl' per permettere all'health check di funzionare
RUN apk add --no-cache curl

WORKDIR /app

COPY package*.json ./

RUN npm install

COPY . .

EXPOSE 3000
EXPOSE 9464

CMD ["node", "server.js"]