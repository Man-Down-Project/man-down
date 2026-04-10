FROM rust:latest

WORKDIR /app

COPY . .

RUN cargo build --release

CMD ["sh", "-c", "until getent hosts mqtt-broker; do sleep 1; done; ./target/release/fog"] 
