FROM python:3.11-slim

WORKDIR /app

RUN pip install paho-mqtt

COPY simulator.py .

CMD ["python", "simulator.py"]
