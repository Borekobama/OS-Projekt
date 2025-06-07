Create a container using:

docker build -t remote-worker .

docker run --rm -e BACKEND_URL=http://188.245.63.120:3000 -e WORKER_NAME="NAME" remote-worker