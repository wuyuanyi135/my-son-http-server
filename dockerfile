FROM ubuntu
EXPOSE 3000
RUN apt update && apt install -y build-essential wget tar

WORKDIR /root

ADD . .

CMD make execute
