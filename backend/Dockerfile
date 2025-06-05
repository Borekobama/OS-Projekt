# Use official Node.js LTS image
FROM node:18-alpine

# Create app directory
WORKDIR /usr/src/app

# Copy package.json and install dependencies
COPY package.json ./
RUN npm install --production

# Copy server code
COPY server.js ./
COPY algorithms ./algorithms

# Expose port 3000
EXPOSE 3000

# Start the backend
CMD ["npm", "start"]
