# Use official Node.js LTS image
FROM node:18-alpine

# Create app directory
WORKDIR /app

# Copy package files
COPY package.json ./

# Install dependencies
RUN npm install

# Copy worker client code
COPY worker-client.js ./

# Expose nothing (optional, not needed here)

# Start the worker client
CMD ["node", "worker-client.js"]