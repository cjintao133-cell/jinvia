# syntax=docker/dockerfile:1
FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends gcc make libc6-dev ca-certificates
WORKDIR /src
COPY . .
RUN make && make lib

FROM debian:bookworm-slim
COPY --from=build /src/jinvia /usr/local/bin/jinvia
COPY --from=build /src/examples /opt/jinvia/examples
WORKDIR /opt/jinvia
ENTRYPOINT ["jinvia"]
CMD ["run","examples/thermostat-guard"]
