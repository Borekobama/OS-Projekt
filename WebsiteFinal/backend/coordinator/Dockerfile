# Use Alpine Linux as the base image
FROM alpine:3

# Install required packages without cache
RUN apk add --no-cache \
	findutils openresolv iptables ip6tables iproute2 wireguard-tools

# Copy the entrypoint script into the container
COPY entrypoint.sh /entrypoint.sh

# Set the entrypoint for the container
ENTRYPOINT ["/entrypoint.sh"]