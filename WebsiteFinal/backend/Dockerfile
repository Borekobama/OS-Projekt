# Use official Node.js LTS image
FROM node:18-alpine

# Create app directory
WORKDIR /usr/src/app

# Copy only package.json
COPY package.json ./

# Install dependencies
RUN npm install --production

# Copy source code
COPY server.js ./
COPY algorithms ./algorithms

# Expose backend port
EXPOSE 3000

# Start the backend directly
CMD ["node", "server.js"]