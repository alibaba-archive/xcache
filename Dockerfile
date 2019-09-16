FROM alpine as builder

RUN apk add build-base
COPY . /opt
RUN cd /opt/server && make -j $(nproc)

FROM alpine

COPY --from=builder /opt/server/xcache /usr/bin/

ENV PORT 20190
EXPOSE $PORT
ENTRYPOINT ["/usr/bin/xcache", "-c", "-p", "$PORT", "-m", "1"]
