FROM node:18-alpine

WORKDIR /app
COPY index.js ./

RUN npm install socket.io

EXPOSE 3100
CMD ["node", "index.js"]