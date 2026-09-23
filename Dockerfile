FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends g++ make && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY . .
RUN ./web/build.sh
FROM python:3.12-slim-bookworm
RUN useradd --create-home --uid 10001 moria
WORKDIR /app
COPY --from=build /app/build-web ./build-web
COPY web ./web
RUN mkdir -p /app/saves && chown moria:moria /app/saves
VOLUME ["/app/saves"]
USER moria
ENV HOST=0.0.0.0 PORT=8080 MAX_ROOMS=8 SAVE_DIR=/app/saves
EXPOSE 8080
CMD ["python3", "web/server.py"]
