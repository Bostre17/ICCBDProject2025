# Usiamo un'immagine base leggera
FROM node:18-alpine
RUN apk add --no-cache curl
# --- FINE MODIFICA ---

WORKDIR /app

COPY package*.json ./

RUN npm install

COPY . .

EXPOSE 3000
EXPOSE 9464

CMD ["node", "server.js"]