# Usiamo un'immagine base leggera
FROM node:18-alpine

# --- MODIFICA CHIAVE ---
# L'immagine Alpine è minimale e non ha 'curl'. Lo installiamo.
# 'apk' è il package manager di Alpine Linux.
# '--no-cache' evita di salvare file non necessari, mantenendo l'immagine piccola.
RUN apk add --no-cache curl
# --- FINE MODIFICA ---

WORKDIR /app

COPY package*.json ./

RUN npm install

COPY . .

EXPOSE 3000
EXPOSE 9464

CMD ["node", "server.js"]