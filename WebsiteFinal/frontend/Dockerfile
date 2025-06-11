# Base: NGINX
FROM nginx:stable-alpine

# Copy custom NGINX config
COPY default.conf /etc/nginx/conf.d/default.conf

# Copy static front-end files into NGINX’s html directory
COPY index.html /usr/share/nginx/html/index.html
COPY style.css /usr/share/nginx/html/
COPY connection.js /usr/share/nginx/html/
COPY coordinator.js /usr/share/nginx/html/
COPY main.js /usr/share/nginx/html/
COPY worker.js /usr/share/nginx/html/
COPY logs.html /usr/share/nginx/html/logs.html
COPY worker.html /usr/share/nginx/html/worker.html
COPY computer.png /usr/share/nginx/html/

# Ensure permissions (optional but good practice)
RUN chmod -R 755 /usr/share/nginx/html

EXPOSE 80
CMD ["nginx", "-g", "daemon off;"]
